#pragma once

#include <QObject>
#include <QMutex>
#include <QSet>
#include <QSharedPointer>

enum class FrameEffectAnimationEquation {
	Linear,
	EaseIn,
	EaseOut,
	EaseInOut
};

class FrameEffectAnimation : public QObject {

	Q_OBJECT

public:
	
	const float value() const {
		return m_value;
	}
	
	const float progress() const {
		return m_progress;
	}
	
	explicit FrameEffectAnimation(float value, QObject* parent = nullptr);
	~FrameEffectAnimation() = default;
	
	void start(uint64_t now, float sv, float ev, float duration, FrameEffectAnimationEquation equation);
	
	void update(uint64_t time);

private:
				
	float m_sv = 0.0f;
	float m_ev = 0.0f;
	double m_duration = 350.0;
	
	FrameEffectAnimationEquation m_equation = FrameEffectAnimationEquation::EaseInOut;
		
	uint64_t m_startedTime = 0;
	uint64_t m_updatedTime = 0;
	float m_progress = 0.0f;
	
	float m_value;
};
