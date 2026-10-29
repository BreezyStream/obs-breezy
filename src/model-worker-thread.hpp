#pragma once

#include "model-worker-thread.hpp"

#include <QThread>

class ModelWorkerThread : public QThread {
	
public:
	
	explicit ModelWorkerThread(QObject *parent = nullptr);
	~ModelWorkerThread() override;
	
};
