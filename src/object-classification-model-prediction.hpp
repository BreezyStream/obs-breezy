#pragma once

#include "prediction.hpp"

#include <QObject>

struct ObjectClassificationModelPredictionInfo {
	int   label;
	float score;
};

class ObjectClassificationModelPrediction : public Prediction {

	Q_OBJECT

public:
	
	int label() {
		return m_label;
	}
	
	float score() {
		return m_score;
	}

	explicit ObjectClassificationModelPrediction(const ObjectClassificationModelPredictionInfo& info, Model* model, QObject* parent = nullptr);

private:
	
	int   m_label;
	float m_score;
		
};
