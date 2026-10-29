#pragma once

#include "model-loader.hpp"
#include "frame.hpp"
#include "prediction.hpp"

#include <QObject>
#include <QVector>

class Breezy;
class ModelLoader;
class ModelWorker : public QObject {
	
	Q_OBJECT

public:

	explicit ModelWorker(Breezy* breezy, QObject* parent = nullptr);
	~ModelWorker();

	void load(const ModelInfo& model);

public slots:
	
	void initialize();
	
signals:
	
	void modelDownloadBegan(Model* model);
	void modelDownloadEnded(Model* model);
	void modelDownloadProgressed(Model* model, qint64 bytes, qint64 total);
	void modelLoaded(Model* model);
	void modelFailed(Model* model);
	void predictionBegan();
	void predictionEnded();
	void predicted(QVector<PredictionPtr> preds);
		
private:

	ModelLoader* m_loader;
	
	QList<Model*> m_models;
	
	void predict(FramePtr frame);
	
private slots:
	
	void onLoadModelRequested(const ModelInfo& info);
	void onLoadModelCancelled(const ModelInfo& info);
	void onModelDownloadBegan(Model* model);
	void onModelDownloadEnded(Model* model);
	void onModelDownloadProgressed(Model* model, qint64 bytes, qint64 total);
	void onModelLoaded(Model* model);
	void onModelFailed(Model* model);
	void onFrameCaptured(FramePtr frame);
		
};
