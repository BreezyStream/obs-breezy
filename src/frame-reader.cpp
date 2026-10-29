#include "frame-reader.hpp"
#include "frame.hpp"

#include <QDebug>

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------

FrameReader::FrameReader(QObject* parent) : QObject(parent) {
	obs_enter_graphics();
	m_texture = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	obs_leave_graphics();
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------

FrameReader::~FrameReader() {
	obs_enter_graphics();
	if (m_texture) gs_texrender_destroy(m_texture);
	if (m_surface) gs_stagesurface_destroy(m_surface);
	obs_leave_graphics();
}

//-----------------------------------------------------------------------------
// Public
//-----------------------------------------------------------------------------

bool
FrameReader::read(obs_source_t* target, uint32_t w, uint32_t h) {

	if (target == nullptr) {
		return false;
	}

	obs_enter_graphics();

	if (m_surface == nullptr ||
		m_sw != w ||
		m_sh != h) {

		if (m_surface) {
			gs_stagesurface_destroy(m_surface);
		}

		m_surface = gs_stagesurface_create(w, h, GS_RGBA);
		m_sw = w;
		m_sh = h;
	}

	obs_leave_graphics();

	gs_texrender_reset(m_texture);

	if (gs_texrender_begin(m_texture, w, h) == false) {
		return false;
	}

	vec4 clear; vec4_zero(&clear);

	gs_clear(GS_CLEAR_COLOR, &clear, 0.f, 0);

	gs_ortho(
		0.f, (float) w,
		0.f, (float) h,
		-100.f,
		+100.f
	);

	uint32_t flags = obs_source_get_output_flags(target);

	bool custom = (flags & OBS_SOURCE_CUSTOM_DRAW) != 0;
	bool async  = (flags & OBS_SOURCE_ASYNC) != 0;

	if (custom == false &&
		async == false) {
		obs_source_default_render(target);
	} else {
		obs_source_video_render(target);
	}

    gs_texrender_end(m_texture);

    gs_texture_t* texture = gs_texrender_get_texture(m_texture);
	
    if (texture == nullptr) {
        return false;
    }

    gs_stage_texture(m_surface, texture);

    return true;
}

bool
FrameReader::copy(Frame& frame) {
	
	uint8_t* ptr = nullptr;
	uint32_t row = 0;

	if (!gs_stagesurface_map(m_surface, &ptr, &row)) {
		return false;
	}

	const uint32_t h = m_sh;
	const uint32_t bytes = frame.stride();

	for (uint32_t y = 0; y < h; y++) {
		memcpy(frame.data() + bytes * y, ptr + row * y, bytes);
	}

	gs_stagesurface_unmap(m_surface);

	return true;
}
