#pragma once

#include <QObject>
#include <QVector>
#include <QSharedPointer>

class Model;
class Prediction : public QObject {

	Q_OBJECT

public:

	Model* model() const {
		return m_model;
	}
	
	explicit Prediction(Model* model, QObject* parent = nullptr);
	virtual ~Prediction() = default;

private:

	Model* m_model;
	
};

using PredictionPtr = QSharedPointer<Prediction>;
