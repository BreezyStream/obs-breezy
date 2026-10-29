#include "frame-effect-filter.hpp"
#include "breezy.hpp"

#include <graphics/graphics.h>
#include <util/platform.h>
 
#include <QDebug>
#include <QMutexLocker>

//-----------------------------------------------------------------------------
// Shader
//-----------------------------------------------------------------------------

static const char* kEffectSource = R"OBS(
uniform float4x4 ViewProj;
uniform texture2d image;

sampler_state textureSampler {
  Filter  = Linear;
  AddressU = Clamp;
  AddressV = Clamp;
};

struct VertInOut {
  float4 pos : POSITION;
  float2 uv  : TEXCOORD0;
};

uniform float  zoomScale;        // >= 1.0
uniform float2 zoomOrigin;       // [0..1]

uniform float  cropRadius;       // > 1 => off
uniform float2 cropOrigin;       // [0..1]

VertInOut VSDefault(VertInOut v_in) {
  VertInOut v_out;
  v_out.pos = mul(float4(v_in.pos.xyz, 1.0), ViewProj);
  v_out.uv  = v_in.uv;
  return v_out;
}

float4 PSDefault(VertInOut v_in) : TARGET {
  // Zoom around origin (avoid div-by-zero)
  float2 uv = (v_in.uv - zoomOrigin) / max(zoomScale, 0.0001) + zoomOrigin;
  uv = clamp(uv, float2(0.0, 0.0), float2(1.0, 1.0));

  float4 c = image.Sample(textureSampler, uv);

  // Circular crop with small feather
  if (cropRadius <= 1.0) {
	float d = distance(v_in.uv, cropOrigin);
	float a = smoothstep(cropRadius, cropRadius * 0.98, d);
	c.a *= (1.0 - a);
  }
  return c;
}

technique Draw {
  pass {
	vertex_shader = VSDefault(v_in);
	pixel_shader  = PSDefault(v_in);
  }
}

)OBS";

//-----------------------------------------------------------------------------
// OBS Callbacks
//-----------------------------------------------------------------------------

static const char*
effect_filter_get_name(void* data) {
	return static_cast<FrameEffectFilter*>(data)->getFilterName();
}

static void
effect_filter_show(void* data) {
	static_cast<FrameEffectFilter*>(data)->onSourceShow();
}

static void
effect_filter_hide(void* data) {
	static_cast<FrameEffectFilter*>(data)->onSourceHide();
}

static void*
effect_filter_create(obs_data_t* config, obs_source_t* source) {
	return new FrameEffectFilter(config, source);
}

static void
effect_filter_destroy(void* data) {
	static_cast<FrameEffectFilter*>(data)->deleteLater();
}

static uint32_t
effect_filter_get_width(void* data) {
	return static_cast<FrameEffectFilter*>(data)->getWidth();
}

static uint32_t
effect_filter_get_height(void* data) {
	return static_cast<FrameEffectFilter*>(data)->getHeight();
}

static void
effect_filter_video_tick(void* data, float time) {
	static_cast<FrameEffectFilter*>(data)->onSourceTick(time);
}

static void
effect_filter_video_render(void* data, gs_effect_t* effect) {
	static_cast<FrameEffectFilter*>(data)->onSourceRender(effect);
}

//-----------------------------------------------------------------------------
// Static API
//-----------------------------------------------------------------------------

void
FrameEffectFilter::registerSource() {

    obs_source_info config = {0};

	config.id           = "breezy_effect_filter";
	config.type         = OBS_SOURCE_TYPE_FILTER;
	config.output_flags = OBS_SOURCE_VIDEO;
	config.get_name     = effect_filter_get_name;
	config.show         = effect_filter_show;
	config.hide         = effect_filter_hide;
	config.create       = effect_filter_create;
	config.destroy      = effect_filter_destroy;
	config.get_width    = effect_filter_get_width;
	config.get_height   = effect_filter_get_height;
	config.video_tick   = effect_filter_video_tick;
	config.video_render = effect_filter_video_render;

	obs_register_source(&config);
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------

FrameEffectFilter::FrameEffectFilter(obs_data_t* config, obs_source_t* source, QObject* parent): QObject(parent), m_config(config), m_source(source) {

	char* err = nullptr;
	
	obs_enter_graphics();
	m_effect = gs_effect_create(kEffectSource, "frame-effect-filter.effect", &err);
	obs_leave_graphics();
			
	if (m_effect == nullptr) {
		qDebug() << "Failed to create effect with error:" << err;
		bfree(err);
		return;
	}
	
	if (err) {
		qDebug() << "Effect warnings:" << err;
		bfree(err);
	}

	m_technique   = gs_effect_get_technique(m_effect, "Draw");
	p_zoomScale   = gs_effect_get_param_by_name(m_effect, "zoomScale");
	p_zoomOrigin  = gs_effect_get_param_by_name(m_effect, "zoomOrigin");
	p_cropRadius  = gs_effect_get_param_by_name(m_effect, "cropRadius");
	p_cropOrigin  = gs_effect_get_param_by_name(m_effect, "cropOrigin");
	
	m_zoomScaleAnimation = new FrameEffectAnimation(1.0f, this);
	m_zoomOriginXAnimation = new FrameEffectAnimation(0.5f, this);
	m_zoomOriginYAnimation = new FrameEffectAnimation(0.5f, this);
	m_cropRadiusAnimation = new FrameEffectAnimation(1.0f, this);
	m_cropOriginXAnimation = new FrameEffectAnimation(0.5f, this);
	m_cropOriginYAnimation = new FrameEffectAnimation(0.5f, this);
	
	connect(Breezy::instance(), qOverload<const FrameZoomEffectInfo&>(&Breezy::applyEffectRequested), this, qOverload<const FrameZoomEffectInfo&>(&FrameEffectFilter::applyEffect));
	connect(Breezy::instance(), qOverload<const FrameCropEffectInfo&>(&Breezy::applyEffectRequested), this, qOverload<const FrameCropEffectInfo&>(&FrameEffectFilter::applyEffect));
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------

FrameEffectFilter::~FrameEffectFilter() {
	if (m_effect) gs_effect_destroy(m_effect);
}

//-----------------------------------------------------------------------------
// Public
//-----------------------------------------------------------------------------

const char*
FrameEffectFilter::getFilterName() {
	return "Breezy Effect";
}

const char*
FrameEffectFilter::getFilterDescription() {
	return "Applies effect on the source.";
}

void
FrameEffectFilter::onSourceShow() {
	m_enabled = true;
}

void
FrameEffectFilter::onSourceHide() {
	m_enabled = false;
}

void
FrameEffectFilter::onSourceTick(float time) {
	update(os_gettime_ns());
}

void
FrameEffectFilter::onSourceRender(gs_effect_t* effect) {
	
	UNUSED_PARAMETER(effect);
	
	obs_source_skip_video_filter(m_source);
	
	if (m_enabled == false ||
		m_effect == nullptr ||
		m_technique == nullptr) {
		obs_source_skip_video_filter(m_source);
		return;
	}
	
	if (obs_source_process_filter_begin(m_source, GS_RGBA, OBS_NO_DIRECT_RENDERING) == false) {
		obs_source_skip_video_filter(m_source);
		return;
	}
	
	const auto currentZoomScale = m_zoomScaleAnimation->value();
	
	float currentZoomOrigin[2] = {
		m_zoomOriginXAnimation->value(),
		m_zoomOriginYAnimation->value()
	};
	
	const auto currentCropRadius = 1.5f; // m_cropRadiusAnimation->value();
	
	float currentCropOrigin[2] = {
		0.5f, // m_cropOriginXAnimation->value(),
		0.5f  // m_cropOriginYAnimation->value()
	};
	
	gs_effect_set_float(p_zoomScale, currentZoomScale);
	gs_effect_set_vec2 (p_zoomOrigin, (vec2*) currentZoomOrigin);
	gs_effect_set_float(p_cropRadius, currentCropRadius);
	gs_effect_set_vec2 (p_cropOrigin, (vec2*) currentCropOrigin);
	
	obs_source_process_filter_tech_end(m_source, m_effect, 0, 0, "Draw");
}

uint32_t
FrameEffectFilter::getWidth() {
	obs_source_t* target = obs_filter_get_target(m_source);
	return target ? obs_source_get_base_width(target) : 0;
}

uint32_t
FrameEffectFilter::getHeight(){
	obs_source_t* target = obs_filter_get_target(m_source);
	return target ? obs_source_get_base_height(target) : 0;
}

//-----------------------------------------------------------------------------
// Private
//-----------------------------------------------------------------------------

bool
FrameEffectFilter::isStreaming() const {
	return obs_frontend_streaming_active();
}

bool
FrameEffectFilter::isRecording() const {
	return obs_frontend_recording_active();
}

void
FrameEffectFilter::update(uint64_t now) {
	QMutexLocker lock(&m_mutex);
	m_zoomScaleAnimation->update(now);
	m_zoomOriginXAnimation->update(now);
	m_zoomOriginYAnimation->update(now);
	/*
	m_cropRadiusAnimation->update(now);
	m_cropOriginXAnimation->update(now);
	m_cropOriginYAnimation->update(now);
	*/
}

void
FrameEffectFilter::applyEffect(const FrameZoomEffectInfo& info) {
	QMutexLocker lock(&m_mutex);
	const auto now = os_gettime_ns();
	m_zoomScaleAnimation->start(now, m_zoomScaleAnimation->value(), info.scale, info.duration, info.equation);
	m_zoomOriginXAnimation->start(now, m_zoomOriginXAnimation->value(), info.originX, info.duration, info.equation);
	m_zoomOriginYAnimation->start(now, m_zoomOriginYAnimation->value(), info.originY, info.duration, info.equation);
}

void
FrameEffectFilter::applyEffect(const FrameCropEffectInfo& info) {
	//QMutexLocker lock(&m_mutex);
}
