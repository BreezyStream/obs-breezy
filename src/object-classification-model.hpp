#pragma once

#include "model.hpp"
#include "onnx-runtime.hpp"

#include <memory>
#include <string>
#include <vector>

#include <QObject>
#include <QVector>

class ObjectClassificationModel : public Model {

	Q_OBJECT

public:

	explicit ObjectClassificationModel(const ModelInfo& info, const QString& path, QObject* parent = nullptr);
	~ObjectClassificationModel();

	bool prepare() override;
	bool predict(const Frame& frame, QVector<PredictionPtr>& preds) override;

	uint32_t inputWidth() const override;
	uint32_t inputHeight() const override;

private:

	int m_threads = 1;

	int m_lastSrcW = -1;
	int m_lastSrcH = -1;
	QVector<int>   m_x0, m_x1;    // cached x indices
	QVector<int>   m_y0, m_y1;
	QVector<float> m_wx;          // cached x weights
	QVector<float> m_wy;

	// Input is NHWC: [1, height, width, channels].
	int m_inputH = 0;
	int m_inputW = 0;
	int m_inputC = 0;
	int m_outputK = 0;

	std::string m_inputName;
	std::string m_outputName;

	std::vector<float> m_input;
	std::vector<float> m_output;

	std::unique_ptr<Ort::Session> m_session;
	std::unique_ptr<Ort::Value> m_inputTensor;
	std::unique_ptr<Ort::Value> m_outputTensor;

	bool process(const Frame& frame);
	bool resolve(const Frame& frame, QVector<PredictionPtr>& preds);
};
