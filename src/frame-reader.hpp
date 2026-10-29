#pragma once

#include <obs.h>

#include <QObject>

class Frame;

class FrameReader : public QObject {
	
	Q_OBJECT

public:

	explicit FrameReader(QObject* parent = nullptr);
	~FrameReader();

	bool read(obs_source_t* target, uint32_t w, uint32_t h);
	bool copy(Frame& frame);

private:

	gs_texrender_t* m_texture = nullptr;
	gs_stagesurf_t* m_surface = nullptr;
	uint32_t m_sw = 0;
	uint32_t m_sh = 0;

};
