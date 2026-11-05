#pragma once

#include "prediction.hpp"

#include <QObject>
#include <QString>

struct ObjectClassificationModelPredictionInfo {
	QString label;
	float   score;
};

class ObjectClassificationModelPrediction : public Prediction {

	Q_OBJECT

public:
	
	QString label() const {
		return m_label;
	}
	
	float score() {
		return m_score;
	}

	explicit ObjectClassificationModelPrediction(const ObjectClassificationModelPredictionInfo& info, Model* model, QObject* parent = nullptr);

private:
	
	QString m_label;
	float   m_score;
		
};
