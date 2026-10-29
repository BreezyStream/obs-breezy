#pragma once

#include "frame-effect-animation.hpp"

#include <obs-module.h>
#include <obs-frontend-api.h>

#include <QObject>
#include <QString>
#include <QList>
#include <QHash>
#include <QMutex>

struct FrameZoomEffectInfo {

	float scale = 0.0f;
	float originX = 0.5f;
	float originY = 0.5f;
	float duration = 250.0f;

	FrameEffectAnimationEquation equation = FrameEffectAnimationEquation::EaseInOut;
};

struct FrameCropEffectInfo {

	float radius = 0.0f;
	float originX = 0.5f;
	float originY = 0.5f;
	float duration = 250.0f;

	FrameEffectAnimationEquation equation = FrameEffectAnimationEquation::EaseInOut;
};

class FrameEffect;
class FrameZoomEffect;
class FrameCropEffect;
class FrameEffectFilter : public QObject {

	Q_OBJECT

public:

	static void registerSource();
	
	explicit FrameEffectFilter(obs_data_t* config, obs_source_t* source, QObject* parent = nullptr);

	~FrameEffectFilter();

	const char* getFilterName();
	const char* getFilterDescription();

	void onSourceShow();
	void onSourceHide();
	void onSourceTick(float time);
	void onSourceRender(gs_effect_t* eff);

	uint32_t getWidth();
	uint32_t getHeight();

public slots:
	
	void applyEffect(const FrameZoomEffectInfo& info);
	void applyEffect(const FrameCropEffectInfo& info);
	
private:

    obs_data_t*   m_config = nullptr;
    obs_source_t* m_source = nullptr;

	// Shader
	gs_effect_t*     m_effect     = nullptr;
	gs_technique_t*  m_technique  = nullptr;
	gs_eparam_t*     p_zoomScale  = nullptr;
	gs_eparam_t*     p_zoomOrigin = nullptr;
	gs_eparam_t*     p_cropRadius = nullptr;
	gs_eparam_t*     p_cropOrigin = nullptr;
	
	FrameEffectAnimation* m_zoomScaleAnimation;
	FrameEffectAnimation* m_zoomOriginXAnimation;
	FrameEffectAnimation* m_zoomOriginYAnimation;
	FrameEffectAnimation* m_cropRadiusAnimation;
	FrameEffectAnimation* m_cropOriginXAnimation;
	FrameEffectAnimation* m_cropOriginYAnimation;
	
	QMutex m_mutex;
	
    bool m_enabled = false;

	bool isStreaming() const;
	bool isRecording() const;
	void update(uint64_t time);

};

