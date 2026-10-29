#include "model-worker-thread.hpp"

ModelWorkerThread::ModelWorkerThread(QObject *parent) : QThread(parent) {
	setObjectName("Model Worker Thread");
}

ModelWorkerThread::~ModelWorkerThread() {
	
}
