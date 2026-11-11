#include "model-loader.hpp"
#include "model.hpp"
#include "object-classification-model.hpp"

#include "plugin-support.h"

#include <obs-module.h>
#include <curl/curl.h>

#include <QObject>
#include <QDir>
#include <QFile>
#include <QFileInfo>
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
	curl_global_init(CURL_GLOBAL_DEFAULT);
}

ModelLoader::~ModelLoader() {

	for (auto& task : m_downloadItems) {

		task->cancelled = true;

		if (task->thread.joinable()) {
			task->thread.join();
		}
	}

	curl_global_cleanup();
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
	name.append("onnx");
	
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
		finishDownloadTask(id, false, "Unable to write file " + task->path);
		return;
	}

	emit downloadBegan(task->model);

	/*
	 * Downloads use libcurl instead of Qt Network, OBS doesn't ship the Qt
	 * TLS backends so HTTPS requests made with Qt fail inside OBS.
	 */

	task->thread = std::thread([this, task] () {
		runDownloadTask(task);
	});
}

void
ModelLoader::runDownloadTask(ModelDownloadTaskPtr task) {

	/*
	 * Runs on the download thread, results are sent back to the main thread
	 * with queued invocations. The file is only written here until the
	 * thread is joined.
	 */

	auto onWrite = [] (char* data, size_t size, size_t count, void* user) -> size_t {

		auto task = static_cast<ModelDownloadTask*>(user);

		qint64 length = static_cast<qint64>(size * count);

		if (task->file->write(data, length) != length) {
			return 0;
		}

		return static_cast<size_t>(length);
	};

	auto onProgress = [] (void* user, curl_off_t total, curl_off_t bytes, curl_off_t, curl_off_t) -> int {

		auto pair = static_cast<std::pair<ModelLoader*, ModelDownloadTask*>*>(user);

		auto loader = pair->first;
		auto task = pair->second;

		if (task->cancelled) {
			return 1;
		}

		auto now = std::chrono::steady_clock::now();

		if (now - task->progressTime < std::chrono::milliseconds(100) && bytes < total) {
			return 0;
		}

		task->progressTime = now;

		auto id = task->id;

		QMetaObject::invokeMethod(loader, [loader, id, bytes, total] () {
			loader->downloadTaskProgress(id, bytes, total);
		}, Qt::QueuedConnection);

		return 0;
	};

	auto url = task->url.toStdString();
	auto agent = std::string("obs-breezy/") + PLUGIN_VERSION;
	auto progress = std::make_pair(this, task.data());

	char message[CURL_ERROR_SIZE] = {};

	CURL* curl = curl_easy_init();

	QString error;

	if (curl) {

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_USERAGENT, agent.c_str());
		curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, message);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
		curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
		curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
		curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 60L);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, static_cast<curl_write_callback>(onWrite));
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, task.data());
		curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, static_cast<curl_xferinfo_callback>(onProgress));
		curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress);
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

		#ifdef _WIN32
			curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_REVOKE_BEST_EFFORT);
		#endif

		CURLcode code = curl_easy_perform(curl);

		if (code != CURLE_OK) {
			error = QString::fromUtf8(message[0] ? message : curl_easy_strerror(code));
		}

		curl_easy_cleanup(curl);

	} else {

		error = "Unable to initialize libcurl.";

	}

	auto id = task->id;

	QMetaObject::invokeMethod(this, [this, id, error] () {
		downloadTaskFinished(id, error);
	}, Qt::QueuedConnection);
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

		blog(LOG_WARNING, "[obs-breezy] Model download failed (%s): %s",
			task->url.toUtf8().constData(),
			error.toUtf8().constData()
		);
		
		emit failed(model);

	}
		
	download();
}

void
ModelLoader::downloadTaskProgress(const QString& key, qint64 bytes, qint64 total) {

	auto task = getDownloadTask(key);
	if (task == nullptr) {
		return;
	}

	emit downloadProgressed(
		task->model,
		bytes,
		total
	);
}

void
ModelLoader::downloadTaskFinished(const QString& id, const QString& error) {
	
	auto task = getDownloadTask(id);
	if (task == nullptr) {
		return;
	}

	if (task->thread.joinable()) {
		task->thread.join();
	}

	if (error.isEmpty() == false) {
		task->file->cancelWriting();
		finishDownloadTask(id, false, error);
		return;
	}

	if (task->file->commit() == false) {
		finishDownloadTask(id, false, "Unable to save file " + task->path);
		return;
	}

	finishDownloadTask(id);
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
