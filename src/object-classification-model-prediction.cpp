#include "object-classification-model-prediction.hpp"

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------

ObjectClassificationModelPrediction::ObjectClassificationModelPrediction(const ObjectClassificationModelPredictionInfo& info, Model* model, QObject* parent) : Prediction(model, parent) {
	m_label = info.label;
	m_score = info.score;
}
