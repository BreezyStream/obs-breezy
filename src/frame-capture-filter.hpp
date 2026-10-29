#pragma once

#include "frame.hpp"
#include "frame-buffer.hpp"
#include "frame-reader.hpp"

#include <obs-module.h>
#include <obs-frontend-api.h>

#include <QObject>

class FrameCaptureFilter : public QObject {

	Q_OBJECT

public:

	static void registerSource();

	explicit FrameCaptureFilter(obs_data_t* config, obs_source_t* source, QObject* parent = nullptr);

	~FrameCaptureFilter();

	const char* getFilterName();
	const char* getFilterDescription();

	void onSourceShow();
	void onSourceHide();
	void onSourceTick(float time);
	void onSourceRender(gs_effect_t* eff);
	void onEvent(enum obs_frontend_event event);

signals:

	void captured(QSharedPointer<Frame> frame);

private:

    obs_data_t*   m_config = nullptr;
    obs_source_t* m_source = nullptr;

    bool m_enabled = false;

	FrameReader* m_reader = nullptr;
	FrameBuffer* m_buffer = nullptr;
	
	bool canCapture() const;
	bool isStreaming() const;
	bool isRecording() const;
	void processFrame(obs_source_t* target);
	
};
