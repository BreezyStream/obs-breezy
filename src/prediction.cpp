#include "prediction.hpp"

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------

Prediction::Prediction(Model* model, QObject* parent) : QObject(parent) {
	m_model = model;
}
