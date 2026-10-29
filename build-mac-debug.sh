#!/bin/bash

export CMAKE_POLICY_VERSION_MINIMUM=3.5

rm -rf build_macos

cmake --preset macos \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
  --fresh

pipx run --spec pbxproj python scripts/mac-remove-agl.py

cmake --build --preset macos --config Debug
