#include "object-classification-model.hpp"
#include "object-classification-model-prediction.hpp"

#include <array>
#include <cmath>

#include <QDebug>

#include <onnxruntime_session_options_config_keys.h>

static inline void sigmoid_inplace(std::vector<float>& v) {
	for (float& x : v) x = float(1.0 / (1.0 + std::exp(-double(x))));
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------

ObjectClassificationModel::ObjectClassificationModel(const ModelInfo& info, const QString& path, QObject* parent) : Model(info, path, parent) {

}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------

ObjectClassificationModel::~ObjectClassificationModel()  {
	m_inputTensor.reset();
	m_outputTensor.reset();
	m_session.reset();
}

//-----------------------------------------------------------------------------
// Public
//-----------------------------------------------------------------------------

bool
ObjectClassificationModel::prepare() {

	if (OnnxRuntime::load() == false) {
		return false;
	}

	try {

		Ort::SessionOptions options;
		options.SetIntraOpNumThreads(m_threads);
		options.SetInterOpNumThreads(1);
		options.SetExecutionMode(ORT_SEQUENTIAL);
		options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

		// Idle threads sleep instead of spinning, OBS needs the CPU more than
		// the model needs the latency.
		options.AddConfigEntry(kOrtSessionOptionsConfigAllowIntraOpSpinning, "0");
		options.AddConfigEntry(kOrtSessionOptionsConfigAllowInterOpSpinning, "0");

#ifdef _WIN32
		std::wstring file = path().toStdWString();
#else
		std::string file = path().toStdString();
#endif

		m_session = std::make_unique<Ort::Session>(OnnxRuntime::env(), file.c_str(), options);

		if (m_session->GetInputCount() < 1 ||
			m_session->GetOutputCount() < 1) {
			qDebug() << "ONNX: Model has no input or output.";
			return false;
		}

		Ort::AllocatorWithDefaultOptions allocator;
		m_inputName = m_session->GetInputNameAllocated(0, allocator).get();
		m_outputName = m_session->GetOutputNameAllocated(0, allocator).get();

		//
		// Input: float32 NHWC, batch of 1.
		//

		// The shape info is a view into the type info, which must outlive it.
		Ort::TypeInfo inputType = m_session->GetInputTypeInfo(0);
		auto inputInfo = inputType.GetTensorTypeAndShapeInfo();
		auto inputShape = inputInfo.GetShape();

		if (inputInfo.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT ||
			inputShape.size() != 4 ||
			inputShape[1] <= 0 ||
			inputShape[2] <= 0 ||
			inputShape[3] <= 2) {
			qDebug() << "ONNX: Unsupported input, expected a float32 [1, H, W, C] tensor.";
			return false;
		}

		m_inputH = int(inputShape[1]);
		m_inputW = int(inputShape[2]);
		m_inputC = int(inputShape[3]);

		//
		// Output: float32 [1, K] or [K], one probability per label.
		//

		Ort::TypeInfo outputType = m_session->GetOutputTypeInfo(0);
		auto outputInfo = outputType.GetTensorTypeAndShapeInfo();
		auto outputShape = outputInfo.GetShape();

		if (outputInfo.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT ||
			outputShape.empty() ||
			outputShape.size() > 2 ||
			outputShape.back() <= 0) {
			qDebug() << "ONNX: Unsupported output, expected a float32 [1, K] tensor.";
			return false;
		}

		m_outputK = int(outputShape.back());

		if (m_outputK != labels().size()) {
			qDebug() << "ONNX: Model has" << m_outputK << "outputs but" << labels().size() << "labels, extra outputs are ignored.";
		}

		//
		// Tensors are bound to buffers owned by the model so inference never
		// allocates.
		//

		m_input.assign(size_t(m_inputH) * m_inputW * m_inputC, 0.f);
		m_output.assign(size_t(m_outputK), 0.f);

		auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

		std::array<int64_t, 4> inputDims = { 1, m_inputH, m_inputW, m_inputC };
		std::array<int64_t, 2> outputDims = { 1, m_outputK };

		m_inputTensor = std::make_unique<Ort::Value>(Ort::Value::CreateTensor<float>(
			memory, m_input.data(), m_input.size(), inputDims.data(), inputDims.size()
		));

		m_outputTensor = std::make_unique<Ort::Value>(Ort::Value::CreateTensor<float>(
			memory, m_output.data(), m_output.size(), outputDims.data() + (outputShape.size() == 2 ? 0 : 1), outputShape.size()
		));

	} catch (const Ort::Exception& e) {
		qDebug() << "ONNX: Failed to load model" << path() << e.what();
		m_session.reset();
		return false;
	}

	qDebug() << "ONNX: Model is ready. Input:" << m_inputH << "x" << m_inputW << "x" << m_inputC << "Outputs:" << m_outputK;

	return true;
}

bool
ObjectClassificationModel::predict(const Frame& frame, QVector<PredictionPtr>& preds) {

	if (m_session == nullptr) {
		return false;
	}

	bool ready = process(frame);
	if (!ready) {
		return false;
	}

	const char* inputNames[] = { m_inputName.c_str() };
	const char* outputNames[] = { m_outputName.c_str() };

	try {
		m_session->Run(Ort::RunOptions{ nullptr }, inputNames, m_inputTensor.get(), 1, outputNames, m_outputTensor.get(), 1);
	} catch (const Ort::Exception& e) {
		qDebug() << "ONNX: Inference failed" << e.what();
		return false;
	}

	return resolve(frame, preds);
}

uint32_t
ObjectClassificationModel::inputWidth() const {
	return uint32_t(m_inputW);
}

uint32_t
ObjectClassificationModel::inputHeight() const {
	return uint32_t(m_inputH);
}

//-----------------------------------------------------------------------------
// Private
//-----------------------------------------------------------------------------

bool
ObjectClassificationModel::process(const Frame& src) {
	
	const int ih = m_inputH;
	const int iw = m_inputW;
	const int ic = m_inputC;

	if (m_input.empty()) {
		return false;
	}

	const int sw = src.w();
	const int sh = src.h();
	const int ss = src.stride();
	const uint8_t* sdata = src.data();// RGBA8888 input assumed

	const bool sameSize = (sw == iw && sh == ih);

	if (!sameSize && (sw != m_lastSrcW || sh != m_lastSrcH)) {

		m_lastSrcW = sw;
		m_lastSrcH = sh;
		m_x0.resize(iw);
		m_x1.resize(iw);
		m_wx.resize(iw);
		m_y0.resize(ih);
		m_y1.resize(ih); m_wy.resize(ih);

		const float fx = float(sw) / float(iw);

		for (int x = 0; x < iw; ++x) {
			const float sx = (x + 0.5f) * fx - 0.5f;
			const int x0 = std::clamp(int(std::floor(sx)), 0, sw - 1);
			m_x0[x] = x0;
			m_x1[x] = std::min(x0 + 1, sw - 1);
			m_wx[x] = sx - std::floor(sx);
		}

		const float fy = float(sh) / float(ih);

		for (int y = 0; y < ih; ++y) {
			const float sy = (y + 0.5f) * fy - 0.5f;
			const int y0 = std::clamp(int(std::floor(sy)), 0, sh - 1);
			m_y0[y] = y0;
			m_y1[y] = std::min(y0 + 1, sh - 1);
			m_wy[y] = sy - std::floor(sy);
		}
	}

	// Write into the model's input buffer
	{

		auto s = specs();

		float* dst         = m_input.data();
		const float scale  = s.inputValueMax - s.inputValueMin;
		const float offset = s.inputValueMin;

		const bool bgr = s.inputColorSpace == "BGR";

		auto toRGB = [&](float r01, float g01, float b01, float a01){
			if (bgr) std::swap(r01, b01);
			*dst++ = r01 * scale + offset;
			*dst++ = g01 * scale + offset;
			*dst++ = b01 * scale + offset;
			if (ic >= 4) *dst++ = (m_inputAlpha ? a01 : 1.0f) * scale + offset;
		};

		if (sameSize) {
			for (int y = 0; y < ih; ++y) {
				const uint8_t* row = sdata + y * ss;
				for (int x = 0; x < iw; ++x) {
					const uint8_t* p = row + 4 * x;      // BGRA/RGBA – adapt if needed
					const float r = p[0] * (1.f/255.f);
					const float g = p[1] * (1.f/255.f);
					const float b = p[2] * (1.f/255.f);
					const float a = p[3] * (1.f/255.f);
					toRGB(r, g, b, a);
				}
			}

		} else {

			for (int y = 0; y < ih; ++y) {

				const uint8_t* r0 = sdata + m_y0[y] * ss;
				const uint8_t* r1 = sdata + m_y1[y] * ss;
				const float wy = m_wy[y];

				for (int x = 0; x < iw; ++x) {

					const uint8_t* p00 = r0 + 4 * m_x0[x];
					const uint8_t* p10 = r0 + 4 * m_x1[x];
					const uint8_t* p01 = r1 + 4 * m_x0[x];
					const uint8_t* p11 = r1 + 4 * m_x1[x];

					auto lerp = [](float a, float b, float t){ return a + (b - a) * t; };
					const float r00 = p00[0]*(1.f/255.f), g00 = p00[1]*(1.f/255.f), b00 = p00[2]*(1.f/255.f), a00 = p00[3]*(1.f/255.f);
					const float r10 = p10[0]*(1.f/255.f), g10 = p10[1]*(1.f/255.f), b10 = p10[2]*(1.f/255.f), a10 = p10[3]*(1.f/255.f);
					const float r01 = p01[0]*(1.f/255.f), g01 = p01[1]*(1.f/255.f), b01 = p01[2]*(1.f/255.f), a01 = p01[3]*(1.f/255.f);
					const float r11 = p11[0]*(1.f/255.f), g11 = p11[1]*(1.f/255.f), b11 = p11[2]*(1.f/255.f), a11 = p11[3]*(1.f/255.f);

					const float bx = lerp(b00, b10, m_wx[x]), by = lerp(b01, b11, m_wx[x]); const float bL = lerp(bx, by, wy);
					const float gx = lerp(g00, g10, m_wx[x]), gy = lerp(g01, g11, m_wx[x]); const float gL = lerp(gx, gy, wy);
					const float rx = lerp(r00, r10, m_wx[x]), ry = lerp(r01, r11, m_wx[x]); const float rL = lerp(rx, ry, wy);
					const float ax = lerp(a00, a10, m_wx[x]), ay = lerp(a01, a11, m_wx[x]); const float aL = lerp(ax, ay, wy);

					toRGB(rL, gL, bL, aL);
				}
			}
		}

		return true;
	}
}

bool
ObjectClassificationModel::resolve(const Frame& frame, QVector<PredictionPtr>& predictions) {

	const int K = m_outputK;
	if (K <= 0) {
		return false;
	}

	std::vector<float> probs(m_output.begin(), m_output.end());

	// Each output is an independent per-label probability (sigmoid head).
	// If they fall outside [0,1] they are logits, convert them.
	for (float v : probs) {
		if (v < 0.f || v > 1.f) {
			sigmoid_inplace(probs);
			break;
		}
	}

	const auto& labels = this->labels();

	// Every label is sent with its score, the widget applies its own
	// threshold and stability.
	for (int i = 0; i < K && i < labels.size(); ++i) {

		ObjectClassificationModelPredictionInfo info;
		info.label = labels[i];
		info.score = probs[i];

		PredictionPtr p = QSharedPointer<ObjectClassificationModelPrediction>(
			new ObjectClassificationModelPrediction(info, this)
		);

		predictions.append(p);
	}

	return true;
}
