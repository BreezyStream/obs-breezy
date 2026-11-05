#pragma once

#include "frame.hpp"
#include "frame-buffer.hpp"
#include "prediction.hpp"

#include <atomic>
#include <climits>
#include <chrono>

#include <QObject>
#include <QString>
#include <QStringList>
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

	// Label ids, in the order of the model's outputs.
	QStringList labels;

	// Label titles, in the same order as the ids.
	QStringList labelTitles;

	// Unversioned ("latest") snapshots rank above every published version.
	int versionMajor = INT_MAX;
	int versionMinor = INT_MAX;
	int versionPatch = INT_MAX;

	double inferenceRate = 10.0;
	double inferenceRateOverride = 0.0; // 0 means no override

	bool enabled = true;

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

	const QStringList& labels() const {
		return m_labels;
	}

	const QStringList& labelTitles() const {
		return m_labelTitles;
	}
	
	double inferenceRate() const {
		return m_inferenceRate;
	}

	double defaultInferenceRate() const {
		return m_defaultInferenceRate;
	}

	bool inferenceRateOverridden() const {
		return m_inferenceRate != m_defaultInferenceRate;
	}
	
	QString path() const {
		return m_path;
	}
	
	bool enabled() const {
		return m_enabled;
	}

	Model* supersededBy() const {
		return m_supersededBy;
	}

	void setEnabled(bool enabled) {
		m_enabled = enabled;
	}

	void setSupersededBy(Model* model) {
		m_supersededBy = model;
	}

	void setInferenceRate(double fps);

	bool isNewerThan(const Model* other) const;

	explicit Model(const ModelInfo& info, const QString& path, QObject* parent = nullptr);
	~Model() = default;

	bool ready();
	bool setup();
		
	void execute(const Frame& frame, QVector<PredictionPtr>& preds);
	
	virtual bool prepare() = 0;
	virtual bool predict(const Frame& frame, QVector<PredictionPtr>& preds) = 0;

	/*
	 * Width and height of the frames the model takes, 0 when unknown.
	 */

	virtual uint32_t inputWidth() const { return 0; }
	virtual uint32_t inputHeight() const { return 0; }
	
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

	QStringList m_labels;
	QStringList m_labelTitles;

	int m_versionMajor = 0;
	int m_versionMinor = 0;
	int m_versionPatch = 0;

	double m_defaultInferenceRate = 10.0;

	QString m_path = "";

	// Written on the worker thread, read by the dock on the main thread.
	std::atomic<double> m_inferenceRate { 10.0 };
	std::atomic<bool>   m_enabled { true };
	std::atomic<Model*> m_supersededBy { nullptr };
	
	clock::duration   m_predictionInterval = clock::duration::zero();
	clock::time_point m_lastPredictionTime = clock::time_point {};
	clock::time_point m_nextPredictionTime = clock::time_point {};
	
};
