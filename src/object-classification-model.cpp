#include "object-classification-model.hpp"
#include "object-classification-model-prediction.hpp"

#include <QDebug>

#include "tensorflow/lite/delegates/xnnpack/xnnpack_delegate.h"
#include "tensorflow/lite/kernels/register.h"

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

	if (m_delegate) {
		TfLiteXNNPackDelegateDelete(m_delegate);
		m_delegate = nullptr;
	}

	m_inter.reset();
	m_model.reset();
}

//-----------------------------------------------------------------------------
// Public
//-----------------------------------------------------------------------------

bool
ObjectClassificationModel::prepare() {
	qDebug() << "Prepare frame";
	m_model = tflite::FlatBufferModel::BuildFromFile(path().toStdString().c_str());

	if (m_model == nullptr) {
		qDebug() << "Tensorflow: Failed to load model file" << path();
		return false;
	}

	tflite::ops::builtin::BuiltinOpResolver resolver;
	tflite::InterpreterBuilder builder(*m_model, resolver);
	builder(&m_inter);

	if (m_inter == nullptr) {
		qDebug() << "Tensorflow: Failed to create interpreter.";
		return false;
	}

	if (m_threads > 0) {
		m_inter->SetNumThreads(m_threads);
	}

	if (m_xnnpack) {

		TfLiteXNNPackDelegateOptions opts = TfLiteXNNPackDelegateOptionsDefault();
		opts.num_threads = 1;

		m_delegate = TfLiteXNNPackDelegateCreate(&opts);

		if (m_delegate) {
			if (m_inter->ModifyGraphWithDelegate(m_delegate) != kTfLiteOk) {
				qDebug() << "Tensorflow: XNNPACK delegate attach failed; continuing on CPU.";
				TfLiteXNNPackDelegateDelete(m_delegate);
				m_delegate = nullptr;
			}
		}
	}

	if (m_inter->AllocateTensors() != kTfLiteOk) {
		qDebug() << "Tensorflow: Failed to allocate tensors.";
		return false;
	}

	const auto& inputs = m_inter->inputs();

	if (inputs.empty()) {
		qDebug() << "Tensorflow: Model has no input tensors.";
		return false;
	}

	auto* in = m_inter->tensor(inputs[0]);

	qDebug()
		<< "Tensorflow: Model is ready. Input idx: %d type: %d dims: %d x %d x %d x %d"
		<< inputs[0]
		<< in->type
		<< (in->dims && in->dims->size > 0 ? in->dims->data[0] : -1)
		<< (in->dims && in->dims->size > 1 ? in->dims->data[1] : -1)
		<< (in->dims && in->dims->size > 2 ? in->dims->data[2] : -1)
		<< (in->dims && in->dims->size > 3 ? in->dims->data[3] : -1);

	return true;
}

bool
ObjectClassificationModel::predict(const Frame& frame, QVector<PredictionPtr>& preds) {

	bool ready = process(frame);
	if (!ready) {
		return false;
	}

	TfLiteStatus status = m_inter->Invoke();

	if (status != kTfLiteOk) {
		return false;
	}

	return resolve(frame, preds);
}

//-----------------------------------------------------------------------------
// Private
//-----------------------------------------------------------------------------

bool
ObjectClassificationModel::process(const Frame& src) {
	
	auto inter = m_inter.get();
	if (!inter) {
		return false;
	}

	const int inputTensorIndex = inter->inputs()[0];

	auto input = inter->tensor(inputTensorIndex);
	if (!input ||
		!input->dims) {
		return false;
	}

	const int ib = input->dims->data[0];
	const int ih = input->dims->data[1];
	const int iw = input->dims->data[2];
	const int ic = input->dims->data[3];


	if (ib != 1 ||
		ic <= 2) {
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

	// Write into model's tensor depending on dtype
	if (input->type == kTfLiteFloat32) {

		auto s = specs();

		float* dst         = input->data.f;
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

	return false;
}

bool
ObjectClassificationModel::resolve(const Frame& frame, QVector<PredictionPtr>& predictions) {

	auto inter = m_inter.get();
	if (!inter) {
		return false;
	}

	const auto& outs = inter->outputs();

	if (outs.size() < 1) {
		return false;
	}

	const int outIdx = outs[0];
	const TfLiteTensor* out = inter->tensor(outIdx);

	if (!out ||
		!out->dims) {
		return false;
	}

	int rank = out->dims->size;
	if (rank != 2 &&
		rank != 1) {
		return false;
	}

	int K = (rank == 2) ? out->dims->data[1] : out->dims->data[0];
	if (K <= 0) {
		return false;
	}

	std::vector<float> probs(K, 0.f);

	 if (out->type == kTfLiteFloat32) {
	   const float* p = out->data.f;
	   if (rank == 2) p += 0; // [1,K]
	   std::copy(p, p + K, probs.begin());
	 } else if (out->type == kTfLiteUInt8 || out->type == kTfLiteInt8) {
	   const float scale = out->params.scale;
	   const int   zp    = out->params.zero_point;
	   if (scale <= 0.f) return false;

	   if (out->type == kTfLiteUInt8) {
		 const uint8_t* q = out->data.uint8;
		 for (int i=0;i<K;++i) probs[i] = scale * (int(q[i]) - zp);
	   } else {
		 const int8_t* q = out->data.int8;
		 for (int i=0;i<K;++i) probs[i] = scale * (int(q[i]) - zp);
	   }
	 } else {
	   qDebug() << "Tensorflow: Unsupported output dtype:" << out->type;
	   return false;
	 }

	// Each output is an independent per-label probability (sigmoid head).
	// If they fall outside [0,1] they are logits, convert them.
	for (float v : probs) {
		if (v < 0.f || v > 1.f) {
			sigmoid_inplace(probs);
			break;
		}
	}

	// Every label is sent with its score, the widget applies its own
	// threshold and stability.
	for (int i = 0; i < K; ++i) {

		ObjectClassificationModelPredictionInfo info;
		info.label = i;
		info.score = probs[i];

		PredictionPtr p = QSharedPointer<ObjectClassificationModelPrediction>(
			new ObjectClassificationModelPrediction(info, this)
		);

		predictions.append(p);
	}

	return true;
}
