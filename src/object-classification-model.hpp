#pragma once

#include "model.hpp"

#include <QObject>
#include <QVector>

#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/model.h"

class ObjectClassificationModel : public Model {

	Q_OBJECT

public:

	explicit ObjectClassificationModel(const ModelInfo& info, const QString& path, QObject* parent = nullptr);
	~ObjectClassificationModel();

	bool prepare() override;
	bool predict(const Frame& frame, QVector<PredictionPtr>& preds) override;
	
private:
	
	int  m_threads = 1;
	bool m_xnnpack = true;
	
	int m_lastSrcW = -1;
	int m_lastSrcH = -1;
	QVector<int>   m_x0, m_x1;    // cached x indices
	QVector<int>   m_y0, m_y1;
	QVector<float> m_wx;          // cached x weights
	QVector<float> m_wy;
	
	std::unique_ptr<tflite::FlatBufferModel> m_model;
	std::unique_ptr<tflite::Interpreter> m_inter;
	TfLiteDelegate* m_delegate = nullptr;
	
	bool process(const Frame& frame);
	bool resolve(const Frame& frame, QVector<PredictionPtr>& preds);
};
