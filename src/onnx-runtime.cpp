#include "onnx-runtime.hpp"

#include <obs-module.h>

#include <mutex>

#include <QDebug>
#include <QDir>
#include <QFileInfo>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

std::once_flag gLoadOnce;
std::unique_ptr<Ort::Env> gEnv;
QString gError;
bool gLoaded = false;

using GetApiBase = const OrtApiBase* (ORT_API_CALL*)();

GetApiBase resolve(const QString& path) {

#ifdef _WIN32

	// Absolute path plus LOAD_WITH_ALTERED_SEARCH_PATH: the library and
	// anything it loads come from the plugin folder, never from another
	// plugin's.
	HMODULE handle = LoadLibraryExW(
		reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(path).utf16()),
		nullptr,
		LOAD_WITH_ALTERED_SEARCH_PATH
	);

	if (handle == nullptr) {
		gError = QString("Unable to load %1 (error %2)").arg(path).arg(GetLastError());
		return nullptr;
	}

	return reinterpret_cast<GetApiBase>(GetProcAddress(handle, "OrtGetApiBase"));

#else

	// RTLD_LOCAL keeps its symbols out of the global namespace so they can't
	// satisfy another plugin's lookups.
	void* handle = dlopen(path.toUtf8().constData(), RTLD_NOW | RTLD_LOCAL);

	if (handle == nullptr) {
		gError = QString("Unable to load %1 (%2)").arg(path, dlerror());
		return nullptr;
	}

	return reinterpret_cast<GetApiBase>(dlsym(handle, "OrtGetApiBase"));

#endif
}

} // namespace

//------------------------------------------------------------------------------
// Public
//------------------------------------------------------------------------------

bool
OnnxRuntime::load() {

	std::call_once(gLoadOnce, []() {

		QString path = libraryPath();

		GetApiBase getApiBase = resolve(path);

		if (getApiBase == nullptr) {
			if (gError.isEmpty()) gError = "OrtGetApiBase not found in " + path;
			return;
		}

		const OrtApiBase* base = getApiBase();
		const OrtApi* api = base ? base->GetApi(ORT_API_VERSION) : nullptr;

		if (api == nullptr) {
			gError = QString("ONNX Runtime %1 doesn't provide API version %2").arg(base ? base->GetVersionString() : "?").arg(ORT_API_VERSION);
			return;
		}

		Ort::InitApi(api);

		try {
			gEnv = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "breezy");
		} catch (const Ort::Exception& e) {
			gError = QString("Unable to create the ONNX Runtime environment: %1").arg(e.what());
			return;
		}

		gLoaded = true;

		qDebug() << "ONNX Runtime" << base->GetVersionString() << "loaded from" << path;
	});

	if (gLoaded == false) {
		qDebug() << "ONNX Runtime:" << gError;
	}

	return gLoaded;
}

Ort::Env&
OnnxRuntime::env() {
	return *gEnv;
}

QString
OnnxRuntime::error() {
	return gError;
}

//------------------------------------------------------------------------------
// Private
//------------------------------------------------------------------------------

QString
OnnxRuntime::libraryPath() {

	// Windows: bin/64bit/obs-breezy.dll, the library sits next to it.
	// macOS: obs-breezy.plugin/Contents/MacOS/obs-breezy, the library is in
	// Contents/Frameworks.
	QFileInfo binary(QString::fromUtf8(obs_get_module_binary_path(obs_current_module())));

	QDir dir = binary.absoluteDir();

#ifdef __APPLE__
	dir.cd("../Frameworks");
#endif

	return dir.absoluteFilePath(BREEZY_ORT_LIBRARY_NAME);
}
