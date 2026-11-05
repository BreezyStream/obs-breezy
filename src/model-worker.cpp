#include "model.hpp"
#include "model-worker.hpp"
#include "model-loader.hpp"
#include "prediction.hpp"
#include "breezy.hpp"

#include <obs.h>

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------

ModelWorker::ModelWorker(Breezy* breezy, bool unifyVersions, QObject* parent) : QObject(parent) {

	m_breezy = breezy;
	m_unifyVersions = unifyVersions;

	connect(breezy, &Breezy::loadModelRequested, this, &ModelWorker::onLoadModelRequested);
	connect(breezy, &Breezy::loadModelCancelled, this, &ModelWorker::onLoadModelCancelled);
	connect(breezy, &Breezy::modelEnabledRequested, this, &ModelWorker::onModelEnabledRequested);
	connect(breezy, &Breezy::modelInferenceRateRequested, this, &ModelWorker::onModelInferenceRateRequested);
	connect(breezy, &Breezy::unifyVersionsRequested, this, &ModelWorker::onUnifyVersionsRequested);
	connect(breezy, &Breezy::captured, this, &ModelWorker::onFrameCaptured);
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------

ModelWorker::~ModelWorker() {

}

//-----------------------------------------------------------------------------
// Public
//-----------------------------------------------------------------------------

void
ModelWorker::initialize() {

	m_loader = new ModelLoader(this);

	connect(m_loader, &ModelLoader::downloadBegan, this, &ModelWorker::onModelDownloadBegan);
	connect(m_loader, &ModelLoader::downloadEnded, this, &ModelWorker::onModelDownloadEnded);
	connect(m_loader, &ModelLoader::downloadProgressed, this, &ModelWorker::onModelDownloadProgressed);
	connect(m_loader, &ModelLoader::loaded, this, &ModelWorker::onModelLoaded);
	connect(m_loader, &ModelLoader::failed, this, &ModelWorker::onModelFailed);
}

void
ModelWorker::load(const ModelInfo& info) {
	m_loader->load(info);
}

//-----------------------------------------------------------------------------
// Private
//-----------------------------------------------------------------------------

Model*
ModelWorker::findModel(const QString& key) const {

	for (auto model : m_models) {
		if (model->key() == key) {
			return model;
		}
	}

	return nullptr;
}

void
ModelWorker::predict(FramePtr frame) {

	if (m_active.isEmpty()) {
		return;
	}

	emit predictionBegan();

	QVector<PredictionPtr> preds;

	for (auto model : m_active) {
		model->execute(*frame, preds);
	}

	if (preds.size() > 0) {
		emit predicted(preds);
	}

	emit predictionEnded();
}

void
ModelWorker::refresh() {

	/*
	 * When unifying, only the most recent enabled version of each model runs,
	 * the others are marked as superseded by it. Widgets match predictions by
	 * model id, so they keep receiving them from the newer version.
	 */

	QHash<QString, Model*> newest;

	if (m_unifyVersions) {

		for (auto model : m_models) {

			if (model->enabled() == false) {
				continue;
			}

			auto current = newest.value(model->id(), nullptr);
			if (current == nullptr || model->isNewerThan(current)) {
				newest.insert(model->id(), model);
			}
		}
	}

	m_active.clear();

	for (auto model : m_models) {

		Model* supersededBy = nullptr;

		if (m_unifyVersions && model->enabled()) {
			auto current = newest.value(model->id());
			if (current != model) {
				supersededBy = current;
			}
		}

		if (model->enabled() && supersededBy == nullptr) {
			m_active.append(model);
		}

		if (model->supersededBy() != supersededBy) {
			model->setSupersededBy(supersededBy);
			emit modelUpdated(model);
		}
	}

	updateCaptureRate();
}

void
ModelWorker::updateCaptureRate() {

	/*
	 * The detector only captures as often as the fastest active model needs,
	 * frames in between would be discarded by the models anyway.
	 */

	double rate = 0.0;

	for (auto model : m_active) {
		rate = std::max(rate, model->inferenceRate());
	}

	/*
	 * Frames are captured at the models' input size when they all share one,
	 * otherwise at the source size and each model resizes them.
	 */

	uint32_t w = 0;
	uint32_t h = 0;

	for (int i = 0; i < m_active.size(); i++) {

		auto mw = m_active[i]->inputWidth();
		auto mh = m_active[i]->inputHeight();

		if (i == 0) {
			w = mw;
			h = mh;
			continue;
		}

		if (mw != w ||
			mh != h) {
			w = 0;
			h = 0;
			break;
		}
	}

	if (rate != m_breezy->captureRate() ||
		w != m_breezy->captureWidth() ||
		h != m_breezy->captureHeight()) {

		if (w && h) {
			blog(LOG_INFO, "[obs-breezy] Capturing at %.1f FPS, resized on the GPU to %ux%u", rate, w, h);
		} else {
			blog(LOG_INFO, "[obs-breezy] Capturing at %.1f FPS, at the source size", rate);
		}
	}

	m_breezy->setCaptureRate(rate);
	m_breezy->setCaptureSize(w, h);
}

//-----------------------------------------------------------------------------
// Private Slots
//-----------------------------------------------------------------------------

void
ModelWorker::onLoadModelRequested(const ModelInfo& info) {
	load(info);
}

void
ModelWorker::onLoadModelCancelled(const ModelInfo& info) {

}

void
ModelWorker::onModelDownloadBegan(Model* model) {
	emit modelDownloadBegan(model);
}

void
ModelWorker::onModelDownloadEnded(Model* model) {
	emit modelDownloadEnded(model);
}

void
ModelWorker::onModelDownloadProgressed(Model* model, qint64 bytes, qint64 total) {
	emit modelDownloadProgressed(model, bytes, total);
}

void
ModelWorker::onModelLoaded(Model* model) {
	m_models.append(model);
	emit modelLoaded(model);
	refresh();
}

void
ModelWorker::onModelFailed(Model* model) {
	emit modelFailed(model);
}

void
ModelWorker::onModelEnabledRequested(const QString& key, bool enabled) {

	auto model = findModel(key);
	if (model == nullptr ||
		model->enabled() == enabled) {
		return;
	}

	model->setEnabled(enabled);
	emit modelUpdated(model);
	refresh();
}

void
ModelWorker::onModelInferenceRateRequested(const QString& key, double rate) {

	auto model = findModel(key);
	if (model == nullptr) {
		return;
	}

	model->setInferenceRate(rate > 0.0 ? rate : model->defaultInferenceRate());
	emit modelUpdated(model);
	updateCaptureRate();
}

void
ModelWorker::onUnifyVersionsRequested(bool unify) {
	m_unifyVersions = unify;
	refresh();
}

void
ModelWorker::onFrameCaptured(FramePtr frame) {
	predict(frame);
}
