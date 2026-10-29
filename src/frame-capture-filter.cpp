#include "frame-capture-filter.hpp"
#include "breezy.hpp"
#include <plugin-support.h>
#include <obs/obs-websocket-api.h>

#include <QDebug>

//-----------------------------------------------------------------------------
// OBS Callbacks
//-----------------------------------------------------------------------------

static void
capture_filter_event(enum obs_frontend_event event, void *data) {
	return static_cast<FrameCaptureFilter*>(data)->onEvent(event);
}

static const char*
capture_filter_get_name(void* data) {
	return static_cast<FrameCaptureFilter*>(data)->getFilterName();
}

static void
capture_filter_show(void* data) {
	static_cast<FrameCaptureFilter*>(data)->onSourceShow();
}

static void
capture_filter_hide(void* data) {
	static_cast<FrameCaptureFilter*>(data)->onSourceHide();
}

static void*
capture_filter_create(obs_data_t* config, obs_source_t* source) {
	return new FrameCaptureFilter(config, source);
}

static void
capture_filter_destroy(void* data) {
	static_cast<FrameCaptureFilter*>(data)->deleteLater();
}

static void
capture_filter_video_tick(void* data, float time) {
	static_cast<FrameCaptureFilter*>(data)->onSourceTick(time);
}

static void
capture_filter_video_render(void* data, gs_effect_t* effect) {
	static_cast<FrameCaptureFilter*>(data)->onSourceRender(effect);
}

//-----------------------------------------------------------------------------
// Static API
//-----------------------------------------------------------------------------

void
FrameCaptureFilter::registerSource() {

    obs_source_info config = {0};

	config.id           = "breezy_filter";
	config.type         = OBS_SOURCE_TYPE_FILTER;
	config.output_flags = OBS_SOURCE_VIDEO;
	config.get_name     = capture_filter_get_name;
	config.show         = capture_filter_show;
	config.hide         = capture_filter_hide;
	config.create       = capture_filter_create;
	config.destroy      = capture_filter_destroy;
	config.video_tick   = capture_filter_video_tick;
	config.video_render = capture_filter_video_render;

	obs_register_source(&config);
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------

FrameCaptureFilter::FrameCaptureFilter(obs_data_t* config, obs_source_t* source, QObject* parent): QObject(parent), m_config(config), m_source(source) {
	
	const auto w = obs_source_get_base_width(source);
	const auto h = obs_source_get_base_height(source);
	
	m_reader = new FrameReader();
	m_buffer = new FrameBuffer(8, w, h, 4);

	connect(this, &FrameCaptureFilter::captured, Breezy::instance(), &Breezy::onCapture);
	
	obs_frontend_add_event_callback(capture_filter_event, this);
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------

FrameCaptureFilter::~FrameCaptureFilter() {
	
	m_reader->deleteLater();
	m_buffer->deleteLater();
	
	obs_frontend_remove_event_callback(capture_filter_event, this);
}

//-----------------------------------------------------------------------------
// Public
//-----------------------------------------------------------------------------

const char*
FrameCaptureFilter::getFilterName() {
	return "Breezy Detector";
}

const char*
FrameCaptureFilter::getFilterDescription() {
	return "Runs Brezzy ML models on source.";
}

void
FrameCaptureFilter::onSourceShow() {
	m_enabled = true;
}

void
FrameCaptureFilter::onSourceHide() {
	m_enabled = false;
}

void
FrameCaptureFilter::onSourceTick(float time) {

}

void
FrameCaptureFilter::onSourceRender(gs_effect_t* effect) {
	
	UNUSED_PARAMETER(effect);
	
	obs_source_skip_video_filter(m_source);
	
	auto active = canCapture();
	auto target = obs_filter_get_target(m_source);
		
	if (active &&
		target) {
		processFrame(target);
	}
}

void
FrameCaptureFilter::onEvent(enum obs_frontend_event event) {

	if (event == OBS_FRONTEND_EVENT_RECORDING_STARTED ||
		event == OBS_FRONTEND_EVENT_STREAMING_STARTED) {
		//BreezyModule::window->setStatus(BreezyWindow::Active);
	}

	if (event == OBS_FRONTEND_EVENT_RECORDING_STOPPED ||
		event == OBS_FRONTEND_EVENT_STREAMING_STOPPED) {
		//BreezyModule::window->setStatus(BreezyWindow::Inactive);
	}
}

//-----------------------------------------------------------------------------
// Private
//-----------------------------------------------------------------------------

bool
FrameCaptureFilter::canCapture() const {
	
    if (m_enabled == false) {
		return false;
	}
    /*
	if (isStreaming() == false &&
		isRecording() == false) {
		return false;
	}
     */
	
	return true;
}

bool
FrameCaptureFilter::isStreaming() const {
	return obs_frontend_streaming_active();
}

bool
FrameCaptureFilter::isRecording() const {
	return obs_frontend_recording_active();
}

void
FrameCaptureFilter::processFrame(obs_source_t* target) {
		
	const auto w = obs_source_get_base_width(target);
	const auto h = obs_source_get_base_height(target);

	auto buffer = m_buffer->acquire();
	if (buffer == nullptr) {
		return;
	}
	
	if (m_reader->read(target, w, h) == false) {
		return;
	}
	
	buffer->resize(w, h, 4);

	if (m_reader->copy(*buffer) == false) {
		return;
	}
	
	emit captured(buffer);
}
