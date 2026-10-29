#pragma once

#include "model.hpp"

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <QSaveFile>
#include <QScopedPointer>
#include <QList>

struct ModelDownloadTask {
	QString id;
	QString url;
	QString path;
	Model* model;
	QList<QMetaObject::Connection> connections;
	QScopedPointer<QSaveFile> file;
	QNetworkReply* reply = nullptr;
};

using ModelDownloadTaskPtr = QSharedPointer<ModelDownloadTask>;

class Model;
class ModelLoader : public QObject {

  Q_OBJECT

public:

	explicit ModelLoader(QObject* parent = nullptr);

	void load(const ModelInfo& info);
	
signals:
	
	void downloadBegan(Model* model);
	void downloadEnded(Model* model);
	void downloadProgressed(Model* model, qint64 bytes, qint64 total);
	void loaded(Model* model);
	void failed(Model* model);

private:

	QHash<QString, Model*> m_models;
	
	QNetworkAccessManager* m_nam;

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
	void downloadTaskProgress(const QString& id, qint64 bytes, qint64 total);
	void downloadTaskFinished(const QString& id);
	void downloadTaskReceivedData(const QString& id);

private slots:
	
	void onModelLoaded(Model* model);
	void onModelFailed(Model* model);
	
};
