#include "frame-effect-animation.hpp"

#include <util/platform.h>

#include <cmath>

#include <QDebug>

//-----------------------------------------------------------------------------
// Static
//-----------------------------------------------------------------------------

static float apply(FrameEffectAnimationEquation e, float t) {
	
	switch (e) {
	case FrameEffectAnimationEquation::Linear:
		return t;
	case FrameEffectAnimationEquation::EaseIn:
		return t * t;
	case FrameEffectAnimationEquation::EaseOut:
		return 1.f - (1.f - t) * (1.f - t);
	case FrameEffectAnimationEquation::EaseInOut:
		return (t < 0.5f) ? (2.f * t * t) : (1.f - std::pow(-2.f * t + 2.f, 2.f) * 0.5f);
	}
	
	return t;
}

static inline float toMS(uint64_t ns) {
	return double(ns) / 1.0e6;
}

//-----------------------------------------------------------------------------
// Constructors
//-----------------------------------------------------------------------------

FrameEffectAnimation::FrameEffectAnimation(float value, QObject* parent) : QObject(parent) {
	m_sv = value;
	m_ev = value;
	m_value = value;
}

//-----------------------------------------------------------------------------
// Public
//-----------------------------------------------------------------------------

void
FrameEffectAnimation::start(uint64_t now, float sv, float ev, float duration, FrameEffectAnimationEquation equation) {

	m_sv = sv;
	m_ev = ev;
	m_duration = std::max(0.0, double(duration));
	m_equation = equation;
	m_progress = 0.0f;

	m_startedTime = now;
	m_updatedTime = now;
		
	m_value = sv;
}

void
FrameEffectAnimation::update(uint64_t now) {
	
	m_updatedTime = now;
	
	float p = std::clamp(
		toMS(m_updatedTime - m_startedTime) / m_duration,
		0.0,
		1.0
	);
					
	m_value = m_sv + (m_ev - m_sv) * apply(m_equation, p);
}
