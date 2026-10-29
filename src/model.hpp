#pragma once

#include "frame.hpp"
#include "frame-buffer.hpp"
#include "prediction.hpp"

#include <chrono>

#include <QObject>
#include <QString>
#include <QVector>
#include <QFileInfo>

const QString kObjectClassificationModelType = "OBJECT_CLASSIFICATION";

struct ModelSpecs {
	QString inputColorSpace = "";
	float inputValueMin = 0.0f;
	float inputValueMax = 0.0f;
};

struct ModelInfo {
	
	QString id;
	QString key;
	QString url;
	QString type;
	QString title;
	QString infos;
	QString version;

	ModelSpecs specs;
		
	double inferenceRate = 10.0;

};

Q_DECLARE_METATYPE(ModelInfo)

class Model : public QObject {

	Q_OBJECT

public:
		
	using clock = std::chrono::steady_clock;
	
	QString id() const {
		return m_id;
	}
	
	QString key() const {
		return m_key;
	}
	
	QString url() const {
		return m_url;
	}

	QString type() const {
		return m_type;
	}
		
	QString title() const {
		return m_title;
	}
	
	QString infos() const {
		return m_infos;
	}

	QString version() const {
		return m_version;
	}

	const ModelSpecs specs() const {
		return m_specs;
	}
	
	double inferenceRate() const {
		return m_inferenceRate;
	}
	
	QString path() const {
		return m_path;
	}
	
	bool enabled() const {
		return m_enabled;
	}

	void setInferenceRate(double fps) {
		m_inferenceRate = fps;
	}
	
	void setEnabled(bool enabled) {
		m_enabled = enabled;
	}
		
	explicit Model(const ModelInfo& info, const QString& path, QObject* parent = nullptr);
	~Model() = default;

	bool ready();
	bool setup();
		
	void execute(const Frame& frame, QVector<PredictionPtr>& preds);
	
	virtual bool prepare() = 0;
	virtual bool predict(const Frame& frame, QVector<PredictionPtr>& preds) = 0;
	
signals:
	void loaded(Model* model);
	void failed(Model* model);
	
protected:

	bool m_inputAlpha = false;
	
private:

	QString m_id = "";
	QString m_key = "";
	QString m_url = "";
	QString m_type = "";
	QString m_title = "";
	QString m_infos = "";
	QString m_version = "";
	
	ModelSpecs m_specs;
		
	double m_inferenceRate = 10.0;
	
	QString m_path = "";
	
	bool m_enabled = true;
	
	clock::duration   m_predictionInterval = clock::duration::zero();
	clock::time_point m_lastPredictionTime = clock::time_point {};
	clock::time_point m_nextPredictionTime = clock::time_point {};
	
};
