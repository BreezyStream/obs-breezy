#pragma once

#include "plugin-support.h"

#include "model-dock.hpp"
#include "model-worker.hpp"
#include "model-worker-thread.hpp"
#include "frame.hpp"

#include <obs.h>
#include <obs-module.h>
#include <obs/obs-websocket-api.h>

#include <memory>

#include <QString>
#include <QObject>
#include <QVector>
#include <QPointer>
#include <QSharedPointer>

#define obs_log(level, msg, ...) blog(level, "[obs-breezy] " msg, ##__VA_ARGS__)

static constexpr const char* kWebSocketVendor = "breezy.stream";
static constexpr const char* kWebSocketRequestModelLoad = "model.load";
static constexpr const char* kWebSocketRequestModelUnload = "model.unload";
static constexpr const char* kWebSocketRequestEffectApply = "effect.apply";

struct ModelInfo;
struct FrameZoomEffectInfo;
struct FrameCropEffectInfo;

class Model;

class Breezy : public QObject {

	Q_OBJECT

public:

	static Breezy* instance();
	static void load();
	static void unload();

	Q_INVOKABLE void loadModel(const ModelInfo& model);
	Q_INVOKABLE void applyEffect(const FrameZoomEffectInfo& info);
	Q_INVOKABLE void applyEffect(const FrameCropEffectInfo& info);

signals:

	void loadModelRequested(const ModelInfo& info);
	void loadModelCancelled(const ModelInfo& info);
	void applyEffectRequested(const FrameZoomEffectInfo& info);
	void applyEffectRequested(const FrameCropEffectInfo& info);
	void captured(FramePtr frame);

public slots:

	void onCapture(FramePtr frame);

private:

	Q_DISABLE_COPY_MOVE(Breezy)

	static QPointer<Breezy> m_instance;

	ModelDock* m_dock;
	ModelWorker* m_worker;
	ModelWorkerThread* m_thread;

	explicit Breezy(QObject* parent = nullptr);
	~Breezy() override;

private slots:

	void onModelDownloadBegan(Model* model);
	void onModelDownloadEnded(Model* model);
	void onModelDownloadProgressed(Model* model, qint64 bytes, qint64 total);
	void onModelLoaded(Model* model);
	void onModelFailed(Model* model);
	void onPredictionBegan();
	void onPredictionEnded();
	void onPredicted(QVector<PredictionPtr> preds);

};
