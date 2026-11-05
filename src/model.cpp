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
	m_labels = info.labels;
	m_labelTitles = info.labelTitles;

	m_versionMajor = info.versionMajor;
	m_versionMinor = info.versionMinor;
	m_versionPatch = info.versionPatch;

	m_defaultInferenceRate = info.inferenceRate;

	m_path = path;

	m_enabled = info.enabled;

	setInferenceRate(info.inferenceRateOverride > 0.0 ? info.inferenceRateOverride : info.inferenceRate);
}

//------------------------------------------------------------------------------
// Public
//------------------------------------------------------------------------------

void
Model::setInferenceRate(double fps) {

	m_inferenceRate = fps;

	if (fps <= 0.0) {

		m_predictionInterval = clock::duration::zero();

	} else {

		m_predictionInterval = std::chrono::duration_cast<clock::duration>(
			std::chrono::duration<double>(1.0 / fps)
		);

	}

	// Restart the schedule so the new rate applies right away.
	m_nextPredictionTime = clock::time_point {};
}

bool
Model::isNewerThan(const Model* other) const {

	if (m_versionMajor != other->m_versionMajor) return m_versionMajor > other->m_versionMajor;
	if (m_versionMinor != other->m_versionMinor) return m_versionMinor > other->m_versionMinor;

	return m_versionPatch > other->m_versionPatch;
}

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
	
	/*
	 * Frames arrive at about the inference rate, a frame that arrives a bit
	 * early is still used instead of waiting a whole capture for the next.
	 */

	if (now + m_predictionInterval / 4 < m_nextPredictionTime) {
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

