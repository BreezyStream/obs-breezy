#include "model.hpp"
#include "frame.hpp"

#include <QDebug>

//------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------

Model::Model(const ModelInfo& info, const QString& path, QObject* parent) : QObject(parent) {
		
	m_id = info.id;
	m_key = info.key;
	m_url = info.url;
	m_type = info.type;
	m_title = info.title;
	m_infos = info.infos;
	m_version = info.version;
	
	m_specs = info.specs;
		
	m_inferenceRate = info.inferenceRate;
	
	m_path = path;
	
	if (m_inferenceRate <= 0.0) {
		
		m_predictionInterval = clock::duration::zero();
		
	} else {
		
		m_predictionInterval = std::chrono::duration_cast<clock::duration>(
			std::chrono::duration<double>(1.0 / m_inferenceRate)
		);
		
	}
}

//------------------------------------------------------------------------------
// Public
//------------------------------------------------------------------------------

bool
Model::ready() {
	return QFileInfo::exists(path());
}

bool
Model::setup() {
	
	auto ready = prepare();
	
	if (ready) {
		emit loaded(this);
	} else {
		emit failed(this);
	}
	
	return ready;
}

void
Model::execute(const Frame& frame, QVector<PredictionPtr>& preds) {

	if (m_enabled == false) {
		return;
	}
		
	const auto now = clock::now();

	if (m_nextPredictionTime == clock::time_point{ }) {
		m_lastPredictionTime = now;
		m_nextPredictionTime = now + m_predictionInterval;
	}
	
	if (now < m_nextPredictionTime) {
		return;
	}
	
	bool predicted = predict(
		frame,
		preds
	);
	
	if (predicted == false) {
		qDebug() << "Model: Prediction failed.";
	}
	
	const auto late  = now - m_nextPredictionTime;
	const auto steps = late.count() >= 0 ? (late / m_predictionInterval) + 1 : 1;
	m_nextPredictionTime += steps * m_predictionInterval;
	m_lastPredictionTime  = now;
}

