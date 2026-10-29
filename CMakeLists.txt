cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

if(APPLE)
  set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build arch" FORCE)
endif()

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

if(WIN32)
  # Tell TF Lite / XNNPACK we link static libs on Windows
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    TFLITE_STATIC_LIBRARY
    TF_LITE_STATIC_LIBRARY
    TFL_STATIC_LIBRARY
    XNNPACK_DELEGATE_STATIC
  )
endif()

if (MSVC)
  target_link_options(${CMAKE_PROJECT_NAME} PRIVATE /WX:NO /IGNORE:4286)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

#-------------------------------------------------------------------------------
# OBS
#-------------------------------------------------------------------------------

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

#-------------------------------------------------------------------------------
# OBS Frontend API
#-------------------------------------------------------------------------------

find_package(obs-frontend-api REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)

#-------------------------------------------------------------------------------
# QT
#-------------------------------------------------------------------------------

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Network)

target_link_libraries(
  ${CMAKE_PROJECT_NAME} PRIVATE
  Qt6::Core
  Qt6::Widgets
  Qt6::Network
)

target_compile_options(
  ${CMAKE_PROJECT_NAME}
  PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
)

set_target_properties(
  ${CMAKE_PROJECT_NAME}
  PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
)

#-------------------------------------------------------------------------------
# OBS Web Socket
#-------------------------------------------------------------------------------

set(OBS_WS_TAG "5.5.6")
set(OBS_WS_URL "https://raw.githubusercontent.com/obsproject/obs-websocket/${OBS_WS_TAG}/lib/obs-websocket-api.h")

set(OBS_WS_STAGE "${CMAKE_BINARY_DIR}/_deps/obsws/include/obs")
file(MAKE_DIRECTORY "${OBS_WS_STAGE}")
set(OBS_WS_HDR "${OBS_WS_STAGE}/obs-websocket-api.h")

if (EXISTS "${OBS_WS_HDR}")
  file(SIZE "${OBS_WS_HDR}" _old_size)
  if (_old_size EQUAL 0)
    file(REMOVE "${OBS_WS_HDR}")
  endif()
endif()

# Download with diagnostics
file(DOWNLOAD "${OBS_WS_URL}" "${OBS_WS_HDR}"
     TLS_VERIFY ON
     STATUS _obsws_status
     LOG _obsws_log
     SHOW_PROGRESS
)

list(GET _obsws_status 0 _code)
list(GET _obsws_status 1 _msg)

message(STATUS "obs-websocket header download: code=${_code} msg='${_msg}'")

if (NOT _code EQUAL 0)
  message(STATUS "obs-websocket download log:\n${_obsws_log}")
  message(FATAL_ERROR "Failed to download obs-websocket-api.h")
endif()

file(SIZE "${OBS_WS_HDR}" _size)

if (_size EQUAL 0)
  message(STATUS "obs-websocket download log:\n${_obsws_log}")
  message(FATAL_ERROR "Downloaded obs-websocket-api.h is empty")
endif()

# Add include dir
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_BINARY_DIR}/_deps/obsws/include")

#-------------------------------------------------------------------------------
# Sources
#-------------------------------------------------------------------------------

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
  src/plugin-main.cpp
  src/breezy.hpp
  src/breezy.cpp
  src/frame.hpp
  src/frame.cpp
  src/frame-buffer.hpp
  src/frame-buffer.cpp
  src/frame-reader.hpp
  src/frame-reader.cpp
  src/frame-capture-filter.hpp
  src/frame-capture-filter.cpp
  src/frame-effect-filter.hpp
  src/frame-effect-filter.cpp
  src/frame-effect-animation.hpp
  src/frame-effect-animation.cpp
  src/model-dock.hpp
  src/model-dock.cpp
  src/model-dock.ui
  src/model-worker.hpp
  src/model-worker.cpp
  src/model-worker-thread.hpp
  src/model-worker-thread.cpp
  src/model-loader.hpp
  src/model-loader.cpp
  src/model.hpp
  src/model.cpp
  src/object-classification-model.hpp
  src/object-classification-model.cpp
  src/object-classification-model-prediction.hpp
  src/object-classification-model-prediction.cpp
  src/prediction.hpp
  src/prediction.cpp
)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

#-------------------------------------------------------------------------------
# Tensorflow
#-------------------------------------------------------------------------------

set(TFLITE_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tensorflow-src")
set(TFLITE_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tflite-lib")
set(TFLITE_BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tflite-build")

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${TFLITE_SRC_DIR}" "${TFLITE_LIB_DIR}/include")

target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
  $<$<OR:$<C_COMPILER_ID:GNU>,$<C_COMPILER_ID:Clang>,$<C_COMPILER_ID:AppleClang>>:-Wno-unused-parameter -Wno-error=unused-parameter>
  $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-Wno-unused-parameter -Wno-error=unused-parameter>
)

find_package(Threads REQUIRED)

#-------------------------------------------------------------------------------
# Tensorflow Windows
#-------------------------------------------------------------------------------

if(WIN32)

  target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    "${TFLITE_SRC_DIR}"
    "${TFLITE_BUILD_DIR}"
  )

  target_link_libraries(${PROJECT_NAME} PRIVATE
    "${TFLITE_BUILD_DIR}/$<CONFIG>/tensorflow-lite.lib"
    "${TFLITE_BUILD_DIR}/_deps/flatbuffers-build/$<CONFIG>/flatbuffers.lib"
    "${TFLITE_BUILD_DIR}/_deps/XNNPACK-build/$<CONFIG>/XNNPACK.lib"
    "${TFLITE_BUILD_DIR}/_deps/cpuinfo-build/$<CONFIG>/cpuinfo.lib"
    "${TFLITE_BUILD_DIR}/pthreadpool/$<CONFIG>/pthreadpool.lib"
    Threads::Threads
  )

  foreach(CFG IN ITEMS Debug Release RelWithDebInfo MinSizeRel)

    file(GLOB RUY_LIBS_${CFG}
      "${TFLITE_BUILD_DIR}/_deps/ruy-build/**/${CFG}/ruy_*.lib")
    file(GLOB ABSL_LIBS_${CFG}
      "${TFLITE_BUILD_DIR}/_deps/abseil-cpp-build/absl/**/${CFG}/absl_*.lib")
    file(GLOB PTHREAD_LIBS_${CFG}
      "${TFLITE_BUILD_DIR}/pthreadpool/${CFG}/pthreadpool.lib")
    file(GLOB FP16_LIBS_${CFG}
      "${TFLITE_BUILD_DIR}/**/${CFG}/*[Ff][Pp]16*.lib")
    file(GLOB FXDIV_LIBS_${CFG}
      "${TFLITE_BUILD_DIR}/**/${CFG}/fxdiv*.lib")
    file(GLOB CLOG_LIBS_${CFG}
      "${TFLITE_BUILD_DIR}/**/${CFG}/clog*.lib")
    file(GLOB FARMHASH_LIBS_${CFG}
      "${TFLITE_BUILD_DIR}/_deps/farmhash-build/${CFG}/farmhash*.lib")
    file(GLOB FFT2D_LIBS_${CFG}
      "${TFLITE_BUILD_DIR}/_deps/fft2d-build/${CFG}/fft2d_fftsg*.lib")

    target_link_libraries(${PROJECT_NAME} PRIVATE
      $<$<CONFIG:${CFG}>:${RUY_LIBS_${CFG}}>
      $<$<CONFIG:${CFG}>:${ABSL_LIBS_${CFG}}>
      $<$<CONFIG:${CFG}>:${PTHREAD_LIBS_${CFG}}>
      $<$<CONFIG:${CFG}>:${FP16_LIBS_${CFG}}>
      $<$<CONFIG:${CFG}>:${FXDIV_LIBS_${CFG}}>
      $<$<CONFIG:${CFG}>:${CLOG_LIBS_${CFG}}>
      $<$<CONFIG:${CFG}>:${FARMHASH_LIBS_${CFG}}>
      $<$<CONFIG:${CFG}>:${FFT2D_LIBS_${CFG}}>
    )

  endforeach()

endif()

#-------------------------------------------------------------------------------
# Tensorflow Mac
#-------------------------------------------------------------------------------

if(APPLE)

  file(GLOB ABSL_LIBS
    "${TFLITE_BUILD_DIR}/_deps/abseil-cpp-build/libabsl_*.a")
  file(GLOB_RECURSE RUY_LIBS
    "${TFLITE_BUILD_DIR}/**/libruy_*.a")
  file(GLOB_RECURSE FP16_LIB
    "${TFLITE_BUILD_DIR}/**/lib[Ff][Pp]16*.a")
  file(GLOB_RECURSE FXDIV_LIB
    "${TFLITE_BUILD_DIR}/**/libfxdiv*.a")
  file(GLOB_RECURSE CLOG_LIB
    "${TFLITE_BUILD_DIR}/**/libclog*.a")
  file(GLOB_RECURSE FARMHASH_LIB
    "${TFLITE_BUILD_DIR}/**/libfarmhash*.a")
  file(GLOB_RECURSE FFT2D_LIB
    "${TFLITE_BUILD_DIR}/**/libfft2d*.a")

  target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    "${TFLITE_BUILD_DIR}/pthreadpool"
  )

  target_link_libraries(${PROJECT_NAME} PRIVATE
    "${TFLITE_BUILD_DIR}/libtensorflow-lite.a"
    "${TFLITE_BUILD_DIR}/_deps/flatbuffers-build/libflatbuffers.a"
    "${TFLITE_BUILD_DIR}/_deps/xnnpack-build/libXNNPACK.a"
    "${TFLITE_BUILD_DIR}/_deps/cpuinfo-build/libcpuinfo.a"
    "${TFLITE_BUILD_DIR}/pthreadpool/libpthreadpool.a"
    ${RUY_LIBS}
    ${FARMHASH_LIB}
    ${FFT2D_LIB}
    ${FP16_LIB}          # optional
    ${FXDIV_LIB}         # optional
    ${CLOG_LIB}          # optional
    ${ABSL_LIBS}
    Threads::Threads
  )

endif()

#-------------------------------------------------------------------------------
# Deployment Mac
#-------------------------------------------------------------------------------

if(APPLE)

  set(OBS_PLUGINS_DIR "$ENV{HOME}/Library/Application Support/obs-studio/plugins" CACHE PATH "Destination folder for OBS plugins on macOS" )
  set(PLUGIN_BUNDLE "${OBS_PLUGINS_DIR}/${_name}.plugin")
  set(RESTART_OBS_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/mac-restart-obs.sh")

  add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    # deploy (remove old, then copy new)
    COMMAND ${CMAKE_COMMAND} -E echo "Deploying to: ${PLUGIN_BUNDLE}"
    COMMAND ${CMAKE_COMMAND} -E remove -f "${PLUGIN_BUNDLE}"
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${PLUGIN_BUNDLE}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${OBS_PLUGINS_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_BUNDLE_DIR:${CMAKE_PROJECT_NAME}>" "${PLUGIN_BUNDLE}"

    # Codesign
    COMMAND codesign --force --deep --sign - --timestamp=none "${PLUGIN_BUNDLE}"

    # Restart OBS
    COMMAND "${RESTART_OBS_SCRIPT}"

    VERBATIM
  )

endif()

#-------------------------------------------------------------------------------
# Deployment Windows
#-------------------------------------------------------------------------------

if(WIN32)

  set(OBS_PLUGINS_DIR   "$ENV{ProgramFiles}/OBS-Studio/obs-plugins/64bit" CACHE PATH "Destination folder for OBS plugins (64-bit)")

  # Copy the built DLL into the plugins dir, and (optionally) restart OBS
  add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Deploying to: ${OBS_PLUGINS_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${OBS_PLUGINS_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${CMAKE_PROJECT_NAME}>" "${OBS_PLUGINS_DIR}/"

    # Optional: brutal restart for dev workflow; comment out if you prefer manual
    # COMMAND ${CMAKE_COMMAND} -E echo "Restarting OBS..."
    # COMMAND cmd /c "taskkill /IM obs64.exe /F 2>nul || exit /b 0"
    # COMMAND cmd /c "start \"\" \"${OBS_EXE}\""
    # VERBATIM
  )
endif()