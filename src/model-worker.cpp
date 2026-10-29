#include "model.hpp"
#include "model-worker.hpp"
#include "model-loader.hpp"
#include "prediction.hpp"
#include "breezy.hpp"

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------

ModelWorker::ModelWorker(Breezy* breezy, QObject* parent) : QObject(parent) {
	connect(breezy, &Breezy::loadModelRequested, this, &ModelWorker::onLoadModelRequested);
	connect(breezy, &Breezy::loadModelCancelled, this, &ModelWorker::onLoadModelCancelled);
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

void
ModelWorker::predict(FramePtr frame) {
	
	if (m_models.isEmpty()) {
		return;
	}
	
	emit predictionBegan();
	
	QVector<PredictionPtr> preds;
	
	for (auto model : m_models) {
		model->execute(*frame, preds);
	}
	
	if (preds.size() > 0) {
		emit predicted(preds);
	}
	
	emit predictionEnded();
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
}

void
ModelWorker::onModelFailed(Model* model) {
	emit modelFailed(model);
}

void
ModelWorker::onFrameCaptured(FramePtr frame) {
	predict(frame);
}
