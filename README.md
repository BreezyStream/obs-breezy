# Breezy OBS Plugin

The OBS Studio plugin for [Breezy](https://breezy.stream). It runs your Breezy AI models **directly inside OBS**, on any video source, and applies live effects driven by what they see.

Models are trained in the Breezy console or in [Breezy Studio](https://github.com/breezystream/breezy-studio), then streamed to OBS as TensorFlow Lite files. Inference runs locally on your machine. No video frames leave your computer.

## Features

- **Breezy Detector** filter: add it to a source to feed its frames to the loaded models.
- **Breezy Effect** filter: animated zoom effects, triggered remotely.
- **Breezy** dock: shows the loaded models, their download progress and their status.
- **obs-websocket vendor** (`breezy.stream`): the Breezy console loads models, receives predictions and triggers effects through OBS WebSocket.

## Install

Download the installer for your platform from [breezy.stream](https://breezy.stream) or the [Releases](https://github.com/breezystream/obs-breezy/releases) page, then restart OBS.

- **macOS:** open the `.pkg` and follow the installer.
- **Windows:** extract the `.zip` into `C:\ProgramData\obs-studio\plugins\`. Create the folder if it doesn't exist (`ProgramData` is hidden, paste the path into File Explorer's address bar).

| Platform | Requirements |
|---|---|
| macOS 12+ (Apple Silicon) | OBS Studio 31+ |
| Windows 10/11 (x64) | OBS Studio 31+ |

## Usage

1. In OBS, enable the WebSocket server (**Tools → WebSocket Server Settings**).
2. Connect the Breezy console to OBS.
3. Add the **Breezy Detector** filter to the source to analyze (your camera, a game capture…).
4. Add the **Breezy Effect** filter to the source that should react.
5. Load a model from the console. It downloads once, is cached locally, and starts predicting.

## WebSocket API

Vendor name: `breezy.stream`. Call requests with obs-websocket's `CallVendorRequest`.

### `model.load`

Downloads (if needed) and loads one or more models.

```json
{
  "models": [
    {
      "__typename": "ObjectClassificationModelSnapshot",
      "id": "…",
      "title": "My model",
      "infos": "",
      "inferenceRate": 10,
      "version": { "label": "1.0" },
      "files": [{ "type": "TENSORFLOW_LITE_FP32_MODEL", "url": "https://…" }],
      "specs": { "inputColor": "RGB", "inputRange": { "min": -1, "max": 1 } }
    }
  ]
}
```

`inferenceRate` (predictions per second) defaults to `10`. `version` defaults to `latest`. Accepted file types: `TENSORFLOW_LITE_FP32_MODEL`, `TENSORFLOW_LITE_FP16_MODEL`, `TENSORFLOW_LITE_INT8_MODEL`.

### `effect.apply`

```json
{
  "type": "zoom",
  "data": {
    "scale": 1.5,
    "options": { "origin": { "x": 0.5, "y": 0.5 }, "duration": 0.3 },
    "equation": "ease-in-out"
  }
}
```

`equation` is one of `linear`, `ease-in`, `ease-out`, `ease-in-out`.

### `model.predict` (event)

Emitted after each inference.

```json
{
  "predictions": [
    {
      "model": { "id": "…", "type": "OBJECT_CLASSIFICATION", "name": "My model", "version": "1.0" },
      "label": 2,
      "score": 0.93
    }
  ]
}
```

## Building from source

The build is based on the [OBS plugin template](https://github.com/obsproject/obs-plugintemplate) and links TensorFlow Lite 2.16.1 statically.

### Requirements

| Platform | Tools |
|---|---|
| macOS | Xcode 16, CMake 3.28+, [pipx](https://pipx.pypa.io) |
| Windows | Visual Studio 2022, CMake 3.28+ |

### 1. Build TensorFlow Lite (once)

This clones TensorFlow into `tensorflow-src/` and builds TFLite into `tflite-build/` and `tflite-lib/`. It takes a while.

```bash
./setup-mac.sh        # macOS
.\setup-win.ps1       # Windows (PowerShell)
```

### 2. Build the plugin

```bash
./build-mac-release.sh      # or ./build-mac-debug.sh
.\build-win-release.ps1     # or .\build-win-debug.ps1
```

The first configure downloads the OBS sources, obs-deps and Qt 6 listed in [buildspec.json](buildspec.json), plus the obs-websocket API header.

On macOS, `scripts/mac-remove-agl.py` strips the obsolete AGL framework from the generated Xcode project, which newer SDKs no longer ship.

### 3. Install locally

Copy the built plugin to OBS's plugin folder, then restart OBS:

- macOS: `build_macos/Release/obs-breezy.plugin` → `~/Library/Application Support/obs-studio/plugins/`
- Windows: run `cmake --install build_x64 --config Release --prefix release`, then copy `release\obs-breezy` → `C:\ProgramData\obs-studio\plugins\`

On macOS, `scripts/mac-restart-obs.sh` quits and relaunches OBS.

## Project layout

```
src/
  plugin-main.cpp            Module entry point
  breezy.*                   Singleton, WebSocket vendor and requests
  frame-capture-filter.*     "Breezy Detector" filter
  frame-effect-filter.*      "Breezy Effect" filter (+ .effect shader)
  model-dock.*               "Breezy" dock
  model-loader.*             Model download and cache
  model-worker*.*            Inference thread
  object-classification-*    TFLite object classification model
cmake/, build-aux/, .github/ OBS plugin template build system
```

## Contributing

Issues and pull requests are welcome. For anything bigger than a fix, please open an issue first to discuss it.

## License

[GPL-2.0-or-later](LICENSE), like OBS Studio. "Breezy" and the Breezy logo are trademarks of their owner and aren't covered by this license.
