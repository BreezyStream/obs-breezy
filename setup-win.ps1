# Clone TensorFlow
git clone https://github.com/tensorflow/tensorflow.git tensorflow-src
git -C tensorflow-src fetch --tags --depth=1
git -C tensorflow-src checkout v2.16.1

$env:CMAKE_POLICY_VERSION_MINIMUM = "3.5"

# Configure
cmake -S tensorflow-src/tensorflow/lite -B tflite-build `
  -G "Visual Studio 17 2022" -A x64 `
  -DBUILD_SHARED_LIBS=OFF `
  -DTFLITE_ENABLE_XNNPACK=ON `
  -DTFLITE_ENABLE_RUY=ON `
  -DFETCHCONTENT_QUIET=OFF

# Build Debug
cmake --build   tflite-build --config Debug --parallel
cmake --install tflite-build --config Debug --prefix "tflite-lib"

# Build Release
cmake --build   tflite-build --config Release --parallel
cmake --install tflite-build --config Release --prefix "tflite-lib"