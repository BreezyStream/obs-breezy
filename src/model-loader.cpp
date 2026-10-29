#include "model-loader.hpp"
#include "model.hpp"
#include "object-classification-model.hpp"

#include <obs-module.h>

#include <QObject>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkRequest>
#include <QCoreApplication>

#include <QDebug>
#include <QMetaEnum>

//------------------------------------------------------------------------------
// Static Functions
//------------------------------------------------------------------------------

inline QString toString(const char* s)        { return QString::fromUtf8(s); }
inline QString toString(const char* s, int n) { return QString::fromUtf8(s, n); }

static void
mkdir(const QString& filepath) {
	QDir().mkpath(QFileInfo(filepath).absolutePath());
}

//------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------

ModelLoader::ModelLoader(QObject* parent) : QObject(parent) {
	m_nam = new QNetworkAccessManager(this);
}

//------------------------------------------------------------------------------
// Public
//------------------------------------------------------------------------------

void
ModelLoader::load(const ModelInfo &info) {
	
	const auto key = info.key;
	const auto url = info.url;
	
	if (m_models.contains(key)) {
		return;
	}
	
	const auto path = getModelCacheFilePath(info);

	auto model = createModel(info, path);
	if (!model) {
		return;
	}
	
	connect(model, &Model::loaded, this, &ModelLoader::onModelLoaded);
	connect(model, &Model::failed, this, &ModelLoader::onModelFailed);
	
	m_models.insert(key, model);
	
	if (info.version != "latest") {
		
		if (model->ready()) {
			model->setup();
			return;
		}
		
	}
		
	if (m_downloadTasks.contains(key)) {
		return;
	}
	
	auto task = QSharedPointer<ModelDownloadTask>(new ModelDownloadTask);
	task->id = key;
	task->url = url;
	task->path = path;
	task->model = model;
	
	m_downloadItems.insert(key, task);
	m_downloadQueue.append(key);

	download();
}

//------------------------------------------------------------------------------
// Private
//------------------------------------------------------------------------------

Model*
ModelLoader::createModel(const ModelInfo& info, const QString& path) {
	
	if (info.type == kObjectClassificationModelType) {
		return new ObjectClassificationModel(info, path, this);
	}

	qDebug() << "Unsupported model type " << info.type;
	
	return nullptr;
}

QString
ModelLoader::getModelCacheBasePath() const {
	
	auto base = obs_module_config_path("models/");
	auto path = toString(base);
	bfree(base);
	
	return path;
}

QString
ModelLoader::getModelCacheFileName(const ModelInfo& info) const {
	
	QString name = "";
	name.append(info.id);
	name.append("@");
	name.append(info.version);
	name.append(".");
	name.append("tflite");
	
	return name;
}

QString
ModelLoader::getModelCacheFilePath(const ModelInfo& info) const {
	
	QString path = "";
	path.append(getModelCacheBasePath());
	path.append(getModelCacheFileName(info));
	
	return path;
}

ModelDownloadTaskPtr
ModelLoader::getDownloadTask(const QString& key) {
	auto it = m_downloadItems.find(key);
	return (it == m_downloadItems.end()) ? nullptr : it.value();
}

void
ModelLoader::download() {
	while (m_downloadTasks.size() < m_maxConcurrentDownloads && m_downloadQueue.isEmpty() == false) {
		startDownloadTask(m_downloadQueue.front());
	}
}

void
ModelLoader::startDownloadTask(const QString &id) {
	
	m_downloadTasks.append(id);
	m_downloadQueue.removeOne(id);

	auto task = getDownloadTask(id);
	
	mkdir(task->path);

	task->file.reset(new QSaveFile(task->path));

	if (task->file->open(QIODevice::WriteOnly) == false) {
		finishDownloadTask(id, false, "Unable to write file.");
		return;
	}

	emit downloadBegan(task->model);
	
	QNetworkRequest req(QUrl(task->url));

	#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
		req.setTransferTimeout(60000);
		req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
		req.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);
	#endif

	req.setRawHeader("Accept-Encoding", "gzip, deflate, br");

	task->reply = m_nam->get(req);

	task->connections << connect(task->reply, &QNetworkReply::downloadProgress, this,
		[this, id] (qint64 bytes, qint64 total) {
			downloadTaskProgress(id, bytes, total);
		}
	);

	task->connections << connect(task->reply, &QNetworkReply::finished, this,
		[this, id] () {
			downloadTaskFinished(id);
		}
	);

	task->connections << connect(task->reply, &QIODevice::readyRead, this,
		[this, id] () {
			downloadTaskReceivedData(id);
		}
	);
}

void
ModelLoader::finishDownloadTask(const QString& id, bool success, const QString& error) {

	auto task = getDownloadTask(id);

	m_downloadItems.remove(id);
	m_downloadTasks.removeOne(id);
	
	auto model = task->model;
	
	emit downloadEnded(task->model);

	if (success) {
		
		if (model->ready()) {
			model->setup();
		}
		
	} else {
		
		emit failed(model);

	}
		
	download();
}

void
ModelLoader::downloadTaskProgress(const QString& key, qint64 bytes, qint64 total) {

	auto task = getDownloadTask(key);

	emit downloadProgressed(
		task->model,
		bytes,
		total
	);
}

void
ModelLoader::downloadTaskFinished(const QString& id) {
	
	auto task = getDownloadTask(id);

	QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> guard(task->reply);

	for (auto &connection : task->connections) {
		QObject::disconnect(connection);
	}
		
	task->connections.clear();
	
	if (task->reply->error() == QNetworkReply::NoError) {
		
		if (task->file->commit()) {
			finishDownloadTask(id);
		} else {
			finishDownloadTask(id, false, "Commit failed.");
		}
				
		return;
	}
	
	task->file->cancelWriting();

	finishDownloadTask(id, false, "Download failed.");
}

void
ModelLoader::downloadTaskReceivedData(const QString& key) {
	auto task = getDownloadTask(key);
	task->file->write(task->reply->readAll());
}

//------------------------------------------------------------------------------
// Private Slots
//------------------------------------------------------------------------------

void
ModelLoader::onModelLoaded(Model *model) {
	emit loaded(model);
}

void
ModelLoader::onModelFailed(Model *model) {
	emit failed(model);
}
