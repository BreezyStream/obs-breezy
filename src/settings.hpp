#pragma once

#include <QHash>
#include <QString>

struct ModelSettings {
	bool enabled = true;
	double inferenceRate = 0.0; // 0 means no override
};

class Settings {

public:

	Settings();

	bool unifyVersions() const {
		return m_unifyVersions;
	}

	void setUnifyVersions(bool unify);

	ModelSettings model(const QString& key) const;

	void setModelEnabled(const QString& key, bool enabled);
	void setModelInferenceRate(const QString& key, double rate);

private:

	bool m_unifyVersions = true;

	QHash<QString, ModelSettings> m_models;

	QString path() const;

	void load();
	void save() const;
};
