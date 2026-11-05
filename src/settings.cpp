#include "settings.hpp"

#include <obs-module.h>

#include <QDir>
#include <QFileInfo>

static constexpr const char* kSettingsFileName = "settings.json";

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------

Settings::Settings() {
	load();
}

//-----------------------------------------------------------------------------
// Public
//-----------------------------------------------------------------------------

void
Settings::setUnifyVersions(bool unify) {
	m_unifyVersions = unify;
	save();
}

ModelSettings
Settings::model(const QString& key) const {
	return m_models.value(key, ModelSettings {});
}

void
Settings::setModelEnabled(const QString& key, bool enabled) {
	auto settings = model(key);
	settings.enabled = enabled;
	m_models.insert(key, settings);
	save();
}

void
Settings::setModelInferenceRate(const QString& key, double rate) {
	auto settings = model(key);
	settings.inferenceRate = rate > 0.0 ? rate : 0.0;
	m_models.insert(key, settings);
	save();
}

//-----------------------------------------------------------------------------
// Private
//-----------------------------------------------------------------------------

QString
Settings::path() const {
	auto path = obs_module_config_path(kSettingsFileName);
	auto result = QString::fromUtf8(path);
	bfree(path);
	return result;
}

void
Settings::load() {

	auto file = path();

	obs_data_t* data = obs_data_create_from_json_file_safe(file.toUtf8().constData(), "bak");
	if (data == nullptr) {
		return;
	}

	obs_data_set_default_bool(data, "unifyVersions", true);

	m_unifyVersions = obs_data_get_bool(data, "unifyVersions");

	obs_data_array_t* models = obs_data_get_array(data, "models");

	if (models) {

		size_t count = obs_data_array_count(models);

		for (size_t i = 0; i < count; i++) {

			obs_data_t* item = obs_data_array_item(models, i);

			obs_data_set_default_bool(item, "enabled", true);

			ModelSettings settings;
			settings.enabled = obs_data_get_bool(item, "enabled");
			settings.inferenceRate = obs_data_get_double(item, "inferenceRate");

			m_models.insert(QString::fromUtf8(obs_data_get_string(item, "key")), settings);

			obs_data_release(item);
		}

		obs_data_array_release(models);
	}

	obs_data_release(data);
}

void
Settings::save() const {

	auto file = path();

	QDir().mkpath(QFileInfo(file).absolutePath());

	obs_data_t* data = obs_data_create();
	obs_data_set_bool(data, "unifyVersions", m_unifyVersions);

	obs_data_array_t* models = obs_data_array_create();

	for (auto it = m_models.constBegin(); it != m_models.constEnd(); ++it) {

		// Entries back to their defaults aren't worth keeping.
		if (it.value().enabled && it.value().inferenceRate <= 0.0) {
			continue;
		}

		obs_data_t* item = obs_data_create();
		obs_data_set_string(item, "key", it.key().toUtf8().constData());
		obs_data_set_bool(item, "enabled", it.value().enabled);
		obs_data_set_double(item, "inferenceRate", it.value().inferenceRate);
		obs_data_array_push_back(models, item);
		obs_data_release(item);
	}

	obs_data_set_array(data, "models", models);
	obs_data_array_release(models);

	obs_data_save_json_safe(data, file.toUtf8().constData(), "tmp", "bak");
	obs_data_release(data);
}
