#pragma once

#include "plugin-support.h"

#include "model-dock.hpp"
#include "model-worker.hpp"
#include "model-worker-thread.hpp"
#include "frame.hpp"
#include "settings.hpp"

#include <obs.h>
#include <obs-module.h>
#include <obs/obs-websocket-api.h>

#include <atomic>
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

	/*
	 * Frames per second the Breezy Detector captures, the highest inference
	 * rate among the active models. Zero when no model is active. Read from
	 * the render thread, written from the model worker thread.
	 */

	double captureRate() const { return m_captureRate; }
	void setCaptureRate(double rate) { m_captureRate = rate; }

	/*
	 * Size the detector captures frames at, the input size of the active
	 * models when they all share one, so frames are resized only once, like
	 * in training. Zero when it should capture at the source size. Width and
	 * height are packed so they're always read together.
	 */

	uint32_t captureWidth() const { return m_captureSize >> 16; }
	uint32_t captureHeight() const { return m_captureSize & 0xFFFF; }
	void setCaptureSize(uint32_t w, uint32_t h) { m_captureSize = ((w & 0xFFFF) << 16) | (h & 0xFFFF); }

signals:

	void loadModelRequested(const ModelInfo& info);
	void loadModelCancelled(const ModelInfo& info);
	void modelEnabledRequested(const QString& key, bool enabled);
	void modelInferenceRateRequested(const QString& key, double rate);
	void unifyVersionsRequested(bool unify);
	void applyEffectRequested(const FrameZoomEffectInfo& info);
	void applyEffectRequested(const FrameCropEffectInfo& info);
	void captured(FramePtr frame);

public slots:

	void onCapture(FramePtr frame);

private:

	Q_DISABLE_COPY_MOVE(Breezy)

	static QPointer<Breezy> m_instance;

	Settings m_settings;

	ModelDock* m_dock;
	ModelWorker* m_worker;
	ModelWorkerThread* m_thread;

	std::atomic<double> m_captureRate { 0.0 };
	std::atomic<uint32_t> m_captureSize { 0 };

	explicit Breezy(QObject* parent = nullptr);
	~Breezy() override;

private slots:

	void onModelDownloadBegan(Model* model);
	void onModelDownloadEnded(Model* model);
	void onModelDownloadProgressed(Model* model, qint64 bytes, qint64 total);
	void onModelLoaded(Model* model);
	void onModelFailed(Model* model);
	void onModelUpdated(Model* model);
	void onModelEnabledChanged(const QString& key, bool enabled);
	void onModelInferenceRateChanged(const QString& key, double rate);
	void onUnifyVersionsChanged(bool unify);
	void onPredictionBegan();
	void onPredictionEnded();
	void onPredicted(QVector<PredictionPtr> preds);

};
