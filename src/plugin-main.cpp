/*
Breezy OBS Plugin
Copyright (C) 2026 Jean-Philippe Dery <info@breezy.stream>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include <obs-module.h>
#include <plugin-support.h>

#include "breezy.hpp"
#include "frame-capture-filter.hpp"
#include "frame-effect-filter.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

const char *
obs_module_name(void) {
	return "Breezy OBS Plugin";
}

const char *
obs_module_description(void) {
	return "Runs Breezy AI models on your OBS sources and applies their effects live.";
}

bool
obs_module_load(void) {

	obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);

	Breezy::load();
	FrameCaptureFilter::registerSource();
	FrameEffectFilter::registerSource();

	return true;
}

void
obs_module_unload(void) {
    obs_log(LOG_INFO, "plugin unloaded");
	Breezy::unload();
}
