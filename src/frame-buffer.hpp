#pragma once

#include "frame.hpp"

#include <QObject>
#include <QMutex>
#include <QSet>
#include <QSharedPointer>

class FrameBuffer : public QObject {

	Q_OBJECT

public:
		
	explicit FrameBuffer(int size, int w, int h, int ch, QObject* parent = nullptr);
	~FrameBuffer();

	FramePtr acquire();

private:

	int m_size;
	
	QMutex m_mutex;
	QSet<Frame*> m_free;
	QSet<Frame*> m_used;

	void release(Frame* frame);

};
