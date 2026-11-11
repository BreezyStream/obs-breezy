#pragma once

#include <onnxruntime_cxx_api.h>

#include <memory>

#include <QString>

//------------------------------------------------------------------------------
// OnnxRuntime
//------------------------------------------------------------------------------
//
// Loads the ONNX Runtime library bundled with the plugin, by absolute path,
// and initializes the C++ API from it. Other OBS plugins ship their own
// onnxruntime library, loading ours under its own name keeps every plugin on
// the version it was built against.
//

class OnnxRuntime {

public:

	// Loads the runtime once, returns false when it isn't available. Safe to
	// call from any thread.
	static bool load();

	// The shared environment, only valid after a successful load().
	static Ort::Env& env();

	// The reason load() failed, empty otherwise.
	static QString error();

private:

	static QString libraryPath();
};
