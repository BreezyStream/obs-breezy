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

	explicit ModelWorker(Breezy* breezy, bool unifyVersions, QObject* parent = nullptr);
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
	void modelUpdated(Model* model);
	void predictionBegan();
	void predictionEnded();
	void predicted(QVector<PredictionPtr> preds);
		
private:

	Breezy* m_breezy;
	ModelLoader* m_loader;
	
	QList<Model*> m_models;
	QList<Model*> m_active;

	bool m_unifyVersions = true;

	Model* findModel(const QString& key) const;

	void predict(FramePtr frame);
	void updateCaptureRate();
	void refresh();

private slots:
	
	void onLoadModelRequested(const ModelInfo& info);
	void onLoadModelCancelled(const ModelInfo& info);
	void onModelDownloadBegan(Model* model);
	void onModelDownloadEnded(Model* model);
	void onModelDownloadProgressed(Model* model, qint64 bytes, qint64 total);
	void onModelLoaded(Model* model);
	void onModelFailed(Model* model);
	void onModelEnabledRequested(const QString& key, bool enabled);
	void onModelInferenceRateRequested(const QString& key, double rate);
	void onUnifyVersionsRequested(bool unify);
	void onFrameCaptured(FramePtr frame);
		
};
