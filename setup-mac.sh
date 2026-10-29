#!/bin/bash

if [[ ! -d "tensorflow-src" ]]; then
  git clone https://github.com/tensorflow/tensorflow.git tensorflow-src
  git -C tensorflow-src fetch --tags --depth=1
  git -C tensorflow-src checkout v2.16.1
fi

export CMAKE_POLICY_VERSION_MINIMUM=3.5

cmake -S tensorflow-src/tensorflow/lite -B tflite-build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="arm64" \
  -DTFLITE_ENABLE_XNNPACK=ON \
  -DTFLITE_ENABLE_GPU=OFF \
  -DTFLITE_ENABLE_RUY=ON \
  -DOVERRIDABLE_FETCH_CONTENT_abseil-cpp_GIT_REPOSITORY=https://github.com/abseil/abseil-cpp \
  -DOVERRIDABLE_FETCH_CONTENT_abseil-cpp_GIT_TAG=master

# Build Debug
cmake --build   tflite-build --config Debug --parallel
cmake --install tflite-build --config Debug --prefix "tflite-lib"

# Build Release
cmake --build   tflite-build --config Release --parallel
cmake --install tflite-build --config Release --prefix "tflite-lib"