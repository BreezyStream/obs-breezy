#pragma once

#include "model.hpp"

#include <QObject>
#include <QSaveFile>
#include <QScopedPointer>
#include <QSharedPointer>
#include <QHash>
#include <QList>

#include <atomic>
#include <chrono>
#include <thread>

struct ModelDownloadTask {
	QString id;
	QString url;
	QString path;
	Model* model;
	QScopedPointer<QSaveFile> file;
	std::thread thread;
	std::atomic<bool> cancelled = false;
	std::chrono::steady_clock::time_point progressTime;
};

using ModelDownloadTaskPtr = QSharedPointer<ModelDownloadTask>;

class Model;
class ModelLoader : public QObject {

  Q_OBJECT

public:

	explicit ModelLoader(QObject* parent = nullptr);
	~ModelLoader() override;

	void load(const ModelInfo& info);

signals:

	void downloadBegan(Model* model);
	void downloadEnded(Model* model);
	void downloadProgressed(Model* model, qint64 bytes, qint64 total);
	void loaded(Model* model);
	void failed(Model* model);

private:

	QHash<QString, Model*> m_models;

	const int m_maxConcurrentDownloads = 2;
	QList<QString> m_downloadTasks;
	QList<QString> m_downloadQueue;
	QHash<QString, ModelDownloadTaskPtr> m_downloadItems;

	Model* createModel(const ModelInfo& info, const QString& path);

	QString getModelCacheBasePath() const;
	QString getModelCacheFileName(const ModelInfo& info) const;
	QString getModelCacheFilePath(const ModelInfo& info) const;
	ModelDownloadTaskPtr getDownloadTask(const QString& key);

	void download();
	void startDownloadTask(const QString& id);
	void finishDownloadTask(const QString& id, bool success = true, const QString& error = "");
	void runDownloadTask(ModelDownloadTaskPtr task);

private slots:

	void downloadTaskProgress(const QString& id, qint64 bytes, qint64 total);
	void downloadTaskFinished(const QString& id, const QString& error);

	void onModelLoaded(Model* model);
	void onModelFailed(Model* model);

};
