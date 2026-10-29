#include "frame-buffer.hpp"
#include "frame.hpp"

#include <QDebug>
#include <QPointer>
#include <QMutexLocker>

//-----------------------------------------------------------------------------
// Constructors
//-----------------------------------------------------------------------------

FrameBuffer::FrameBuffer(int size, int w, int h, int ch, QObject* parent) : QObject(parent) {
	
	m_size = size;
	m_free.reserve(size);
	m_used.reserve(size);
	
	for (int i = 0; i < size; i++) {
		m_free.insert(new Frame(w, h, ch));
	}
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------

FrameBuffer::~FrameBuffer() {

	if (m_used.isEmpty() == false) {
		qDebug() << "Frame buffer destroyed with" << m_used.size() << "in-use frames.";
	}
	
	for (auto* frame : m_free) delete frame;
	for (auto* frame : m_used) delete frame;
	
	m_free.clear();
	m_used.clear();
}

//-----------------------------------------------------------------------------
// Public
//-----------------------------------------------------------------------------

FramePtr
FrameBuffer::acquire() {

	QMutexLocker lock(&m_mutex);

	if (m_free.empty()) {
		return QSharedPointer<Frame>();
	}
	
	auto* frame = *(m_free.begin());
	
	m_used.insert(frame);
	m_free.remove(frame);
	
	QPointer<FrameBuffer> self(this);
	return QSharedPointer<Frame>(frame, [self](Frame* frame) {
		if (self) self->release(frame);
	});
}

//-----------------------------------------------------------------------------
// Private
//-----------------------------------------------------------------------------

void
FrameBuffer::release(Frame* frame) {
	QMutexLocker lock(&m_mutex);
	m_free.insert(frame);
	m_used.remove(frame);
}
