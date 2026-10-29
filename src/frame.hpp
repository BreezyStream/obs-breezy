#pragma once

#include <memory>

#include <QVector>
#include <QSharedPointer>

class Frame {

public:

	Frame(uint32_t w, uint32_t h, uint32_t ch);
	~Frame();

	uint32_t w() const {
		return m_w;
	}

	uint32_t h() const {
		return m_h;
	}

	uint32_t ch() const {
		return m_ch;
	}

	uint32_t stride() const {
		return m_stride;
	}

    uint8_t* data() {
        return m_data.get();
    }

    const uint8_t* data() const {
		return m_data.get();
    }

	void resize(uint32_t w, uint32_t h, uint32_t ch);

private:

	uint32_t m_w = 0;
	uint32_t m_h = 0;
	uint32_t m_ch = 0;
	uint32_t m_stride = 0;
	uint64_t m_size = 0;
	std::unique_ptr<uint8_t[]> m_data;

};

using FramePtr = QSharedPointer<Frame>;
