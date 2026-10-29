#include "breezy.hpp"

#include "model.hpp"
#include "frame.hpp"
#include "prediction.hpp"
#include "object-classification-model-prediction.hpp"
#include "frame-effect-filter.hpp"

#include <QObject>
#include <QMainWindow>
#include <QSharedPointer>
#include <QApplication>
#include <QThread>

#include <obs.h>
#include <obs-frontend-api.h>
#include <obs/obs-websocket-api.h>
#include <util/platform.h>

QPointer<Breezy> Breezy::m_instance {};

//-----------------------------------------------------------------------------
// Static
//-----------------------------------------------------------------------------

static obs_websocket_vendor g_vendor = nullptr;

//-----------------------------------------------------------------------------
// OBS Callbacks
//-----------------------------------------------------------------------------

void
obs_log_handler(QtMsgType type, const QMessageLogContext &context, const QString &message) {
	obs_log(LOG_INFO, "%s", message.toStdString().c_str());
}

void
obs_websocket_request_model_load(obs_data_t *request_data, obs_data_t *response_data, void *data) {
	
	UNUSED_PARAMETER(response_data);

	qDebug() << "OBS WebSocket Request: model.load";
	
	const auto instance = reinterpret_cast<Breezy*>(data);

	obs_data_array* models = obs_data_get_array(request_data, "models");

	if (models) {

		int count = static_cast<int>(obs_data_array_count(models));

		for (int i = 0; i < count; i++) {

			obs_data_t* item = obs_data_array_item(models, i);

			const char* type  = obs_data_get_string(item, "__typename");

			if (strcmp(type, "ObjectClassificationModelSnapshot") != 0) {
				qDebug() << "Unsupported model type" << type;
				obs_data_release(item);
				continue;
			}

			const char* id    = obs_data_get_string(item, "id");
			const char* title = obs_data_get_string(item, "title");
			const char* infos = obs_data_get_string(item, "infos");

			ModelInfo info;

			info.id = QString(id);
			info.type = kObjectClassificationModelType;
			info.title = QString(title);
			info.infos = QString(infos);

			double inferenceRate = 10;

			if (obs_data_has_user_value(item, "inferenceRate")) inferenceRate = obs_data_get_double(item, "inferenceRate");

			info.inferenceRate = inferenceRate;
			
			if (obs_data_has_user_value(item, "version")) {
				
				obs_data_t* version = obs_data_get_obj(item, "version");
				info.version = QString(obs_data_get_string(version, "label"));
				obs_data_release(version);
				
			} else {
				
				info.version = "latest";

			}
			
			obs_data_array* files = obs_data_get_array(item, "files");

			if (files) {
		
				int count = static_cast<int>(obs_data_array_count(files));
				
				for (int i = 0; i < count; i++) {
					
					obs_data_t* item = obs_data_array_item(files, i);
					
					const char* url  = obs_data_get_string(item, "url");
					const char* type = obs_data_get_string(item, "type");
					
					if (strcmp(type, "TENSORFLOW_LITE_FP32_MODEL") == 0 ||
						strcmp(type, "TENSORFLOW_LITE_FP16_MODEL") == 0 ||
						strcmp(type, "TENSORFLOW_LITE_INT8_MODEL") == 0) {
						info.url = QString(url);
						break;
					}
				}
				
				obs_data_array_release(files);
			}
			
			obs_data_t* specs = obs_data_get_obj(item, "specs");

			if (specs) {

				const char* inputColor = obs_data_get_string(specs, "inputColor");

				info.specs.inputColorSpace = QString(inputColor);

				obs_data_t* inputRange = obs_data_get_obj(specs, "inputRange");

				info.specs.inputValueMin = (float) obs_data_get_double(inputRange, "min");
				info.specs.inputValueMax = (float) obs_data_get_double(inputRange, "max");
								
				obs_data_release(inputRange);

				obs_data_release(specs);
			}

			obs_data_release(item);

			info.key = info.id + "@" + info.version;
			
			QMetaObject::invokeMethod(
				instance,
				"loadModel",
				Qt::QueuedConnection,
				Q_ARG(ModelInfo, info)
			);
		}
		
		obs_data_array_release(models);
	}
}

void
obs_websocket_request_effect(obs_data_t *request_data, obs_data_t *response_data, void *data) {

	UNUSED_PARAMETER(response_data);

	const auto instance = reinterpret_cast<Breezy*>(data);

	auto type = obs_data_get_string(request_data, "type");

	if (strcmp(type, "zoom") == 0) {

		obs_data_t* data = obs_data_get_obj(request_data, "data");

		FrameZoomEffectInfo info;

		if (obs_data_has_user_value(data, "scale")) {
			info.scale = (float) obs_data_get_double(data, "scale");
		}

		if (obs_data_has_user_value(data, "options")) {

			obs_data_t* options = obs_data_get_obj(data, "options");

			if (obs_data_has_user_value(options, "origin")) {
				obs_data_t* origin = obs_data_get_obj(options, "origin");
				if (obs_data_has_user_value(origin, "x")) info.originX = (float) obs_data_get_double(origin, "x");
				if (obs_data_has_user_value(origin, "y")) info.originY = (float) obs_data_get_double(origin, "y");
				obs_data_release(origin);
			}

			if (obs_data_has_user_value(options, "duration")) {
				info.duration = (float) obs_data_get_double(options, "duration");
			}

			if (obs_data_has_user_value(data, "equation")) {

				auto equation = obs_data_get_string(data, "equation");

				if (strcmp(equation, "linear") == 0) {
					info.equation = FrameEffectAnimationEquation::Linear;
				} else if (strcmp(equation, "ease-in") == 0) {
					info.equation = FrameEffectAnimationEquation::EaseIn;
				} else if (strcmp(equation, "ease-out") == 0) {
					info.equation = FrameEffectAnimationEquation::EaseOut;
				} else if(strcmp(equation, "ease-in-out") == 0) {
					info.equation = FrameEffectAnimationEquation::EaseInOut;
				}
			}

			obs_data_release(options);
		}

		obs_data_release(data);

		QMetaObject::invokeMethod(
			instance,
			"applyEffect",
			Qt::QueuedConnection,
			Q_ARG(FrameZoomEffectInfo, info)
	  	);
	}
}

void
obs_register_websocket_vendor_and_requests(Breezy* breezy) {

	g_vendor = obs_websocket_register_vendor(kWebSocketVendor);

	if (g_vendor) {
		obs_websocket_vendor_register_request(g_vendor, kWebSocketRequestModelLoad, obs_websocket_request_model_load, breezy);
		obs_websocket_vendor_register_request(g_vendor, kWebSocketRequestEffectApply, obs_websocket_request_effect, breezy);
	} else {
		qDebug() << "Failed to regsiter socket vendor" << kWebSocketVendor;
	}
}

void
on_frontend_event(enum obs_frontend_event e, void *data) {
	if (e == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
		obs_register_websocket_vendor_and_requests(reinterpret_cast<Breezy*>(data));
	}
}

//-----------------------------------------------------------------------------
// Static Methods
//-----------------------------------------------------------------------------

Breezy*
Breezy::instance() {
	return m_instance.data();
}

void
Breezy::load() {

	qRegisterMetaType<ModelInfo>("ModelInfo");
	qRegisterMetaType<FrameZoomEffectInfo>("FrameZoomEffectInfo");
	qRegisterMetaType<FrameCropEffectInfo>("FrameCropEffectInfo");
	qRegisterMetaType<QVector<FramePtr>>("FramePtrVector");
	qRegisterMetaType<QVector<PredictionPtr>>("PredictionPtrVector");
	qInstallMessageHandler(obs_log_handler);

	if (m_instance == nullptr) {
		m_instance = new Breezy();
	}
}

void
Breezy::unload() {
	if (m_instance) {
		m_instance->deleteLater();
		m_instance = nullptr;
	}
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------

Breezy::Breezy(QObject* parent) : QObject(parent) {

	auto* window = static_cast<QMainWindow*>(obs_frontend_get_main_window());

	QThread::currentThread()->setObjectName("Main Thread");

	m_dock = new ModelDock(window);

	m_worker = new ModelWorker(this);
	m_thread = new ModelWorkerThread(this);
	m_worker->moveToThread(m_thread);

	connect(m_thread, &QThread::started, m_worker, &ModelWorker::initialize);
	connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

	connect(m_worker, &ModelWorker::modelDownloadBegan, this, &Breezy::onModelDownloadBegan);
	connect(m_worker, &ModelWorker::modelDownloadEnded, this, &Breezy::onModelDownloadEnded);
	connect(m_worker, &ModelWorker::modelDownloadProgressed, this, &Breezy::onModelDownloadProgressed);
	connect(m_worker, &ModelWorker::modelLoaded, this, &Breezy::onModelLoaded);
	connect(m_worker, &ModelWorker::modelFailed, this, &Breezy::onModelFailed);
	connect(m_worker, &ModelWorker::predictionBegan, this, &Breezy::onPredictionBegan);
	connect(m_worker, &ModelWorker::predictionEnded, this, &Breezy::onPredictionEnded);
	connect(m_worker, &ModelWorker::predicted, this, &Breezy::onPredicted);

	m_thread->start();

	obs_frontend_add_event_callback(on_frontend_event, this);

	obs_frontend_add_dock_by_id("breezy_model_dock", "Breezy", m_dock);
}

Breezy::~Breezy() {

	if (m_thread) {
		m_thread->quit();
		m_thread->wait();
	}

	m_instance = nullptr;
}

//-----------------------------------------------------------------------------
// Public API
//-----------------------------------------------------------------------------

void
Breezy::loadModel(const ModelInfo& info) {
	emit loadModelRequested(info);
}

void
Breezy::applyEffect(const FrameZoomEffectInfo &info) {
	emit applyEffectRequested(info);
}

void
Breezy::applyEffect(const FrameCropEffectInfo &info) {
	emit applyEffectRequested(info);
}

//-----------------------------------------------------------------------------
// Private Slots
//-----------------------------------------------------------------------------

void
Breezy::onCapture(FramePtr buffer) {
	emit captured(buffer);
}

void
Breezy::onModelDownloadBegan(Model* model) {
	m_dock->updateModelTableItem(model, ModelDock::ModelStatus::DOWNLOAD_BEGAN);
}

void
Breezy::onModelDownloadEnded(Model* model) {
	m_dock->updateModelTableItem(model, ModelDock::ModelStatus::DOWNLOAD_ENDED);
}

void
Breezy::onModelDownloadProgressed(Model* model, qint64 bytes, qint64 total) {
	m_dock->updateModelTableItem(model, ModelDock::ModelStatus::DOWNLOADING, (float) bytes / (float) total);
}

void
Breezy::onModelLoaded(Model* model) {
	m_dock->updateModelTableItem(model, ModelDock::ModelStatus::ACTIVE);
}

void
Breezy::onModelFailed(Model* model) {
	m_dock->updateModelTableItem(model, ModelDock::ModelStatus::FAILED);
}

void
Breezy::onPredictionBegan() {

}

void
Breezy::onPredictionEnded() {

}

void
Breezy::onPredicted(QVector<PredictionPtr> preds) {

	if (preds.isEmpty()) {
		return;
	}
	
	obs_data_t* payload = obs_data_create();

	obs_data_array_t* items = obs_data_array_create();

	for (auto pred : preds) {

		obs_data_t* entry = obs_data_create();
		obs_data_t* model = obs_data_create();

		obs_data_set_string(model, "id", pred->model()->id().toStdString().c_str());
		obs_data_set_string(model, "type", pred->model()->type().toStdString().c_str());
		obs_data_set_string(model, "name", pred->model()->title().toStdString().c_str());
		obs_data_set_string(model, "version", pred->model()->version().toStdString().c_str());

		obs_data_set_obj(entry, "model", model);

		if (auto value = qSharedPointerDynamicCast<ObjectClassificationModelPrediction>(pred)) {

			obs_data_set_double(entry, "score", value->score());
			obs_data_set_double(entry, "label", value->label());

		}

		obs_data_array_push_back(
			items,
		 	entry
		 );

		obs_data_release(model);
		obs_data_release(entry);
	}

	obs_data_set_array(payload, "predictions", items);
	obs_data_array_release(items);

	obs_websocket_vendor_emit_event(g_vendor, "model.predict", payload);

	obs_data_release(payload);
}
