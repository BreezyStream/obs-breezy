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

find_package(Qt6 REQUIRED COMPONENTS Widgets Core)

target_link_libraries(
  ${CMAKE_PROJECT_NAME} PRIVATE
  Qt6::Core
  Qt6::Widgets
)

#-------------------------------------------------------------------------------
# CURL
#-------------------------------------------------------------------------------

# Model downloads use libcurl, OBS doesn't ship the Qt TLS backends so HTTPS
# requests made with Qt Network fail inside OBS.

find_package(CURL REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE CURL::libcurl)

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
  src/model-dock-item.hpp
  src/model-dock-item.cpp
  src/toggle-switch.hpp
  src/toggle-switch.cpp
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
  src/onnx-runtime.hpp
  src/onnx-runtime.cpp
  src/prediction.hpp
  src/prediction.cpp
  src/settings.hpp
  src/settings.cpp
)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

#-------------------------------------------------------------------------------
# ONNX Runtime
#-------------------------------------------------------------------------------

# The prebuilt runtime is shipped next to the plugin under a Breezy specific
# name and loaded at runtime by absolute path (see src/onnx-runtime.cpp). Other
# OBS plugins bundle their own onnxruntime library, a shared name would make
# the process resolve one plugin's copy for another.

set(ORT_VERSION "1.31.1")

if(APPLE)
  set(ORT_PACKAGE "onnxruntime-osx-arm64-${ORT_VERSION}")
  set(ORT_ARCHIVE "${ORT_PACKAGE}.tgz")
  set(ORT_SHA256 "2e1344a1926ee50eb25b38153cb6992c3567515aef0e677a1a520a1798896d6f")
  set(ORT_LIBRARY_SOURCE "lib/libonnxruntime.${ORT_VERSION}.dylib")
  set(ORT_LIBRARY_NAME "libbreezy-onnxruntime.dylib")
elseif(WIN32)
  set(ORT_PACKAGE "onnxruntime-win-x64-${ORT_VERSION}")
  set(ORT_ARCHIVE "${ORT_PACKAGE}.zip")
  set(ORT_SHA256 "5869e26bb25314375f2d5ef1877e3ea06b1543d6c710e6e4a1ec2f7177ab4b2c")
  set(ORT_LIBRARY_SOURCE "lib/onnxruntime.dll")
  set(ORT_LIBRARY_NAME "breezy-onnxruntime.dll")
else()
  message(FATAL_ERROR "ONNX Runtime is only set up for macOS and Windows")
endif()

set(ORT_DEPS_DIR "${CMAKE_SOURCE_DIR}/.deps/onnxruntime")
set(ORT_ROOT "${ORT_DEPS_DIR}/${ORT_PACKAGE}")
set(ORT_LIBRARY "${ORT_DEPS_DIR}/${ORT_LIBRARY_NAME}")

if(NOT EXISTS "${ORT_ROOT}/include/onnxruntime_cxx_api.h")

  file(DOWNLOAD
    "https://github.com/microsoft/onnxruntime/releases/download/v${ORT_VERSION}/${ORT_ARCHIVE}"
    "${ORT_DEPS_DIR}/${ORT_ARCHIVE}"
    EXPECTED_HASH SHA256=${ORT_SHA256}
    TLS_VERIFY ON
    SHOW_PROGRESS
  )

  file(ARCHIVE_EXTRACT INPUT "${ORT_DEPS_DIR}/${ORT_ARCHIVE}" DESTINATION "${ORT_DEPS_DIR}")
  file(REMOVE "${ORT_DEPS_DIR}/${ORT_ARCHIVE}")
  file(REMOVE "${ORT_LIBRARY}")

endif()

if(NOT EXISTS "${ORT_LIBRARY}")

  file(COPY_FILE "${ORT_ROOT}/${ORT_LIBRARY_SOURCE}" "${ORT_LIBRARY}")

  # The install name is what dyld matches loaded images on, give the renamed
  # copy its own.
  if(APPLE)
    execute_process(
      COMMAND install_name_tool -id "@rpath/${ORT_LIBRARY_NAME}" "${ORT_LIBRARY}"
      COMMAND_ERROR_IS_FATAL ANY
    )
  endif()

endif()

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${ORT_ROOT}/include")

# The API is fetched from the loaded library, nothing links against it.
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
  ORT_API_MANUAL_INIT
  BREEZY_ORT_LIBRARY_NAME="${ORT_LIBRARY_NAME}"
)

target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
  $<$<OR:$<C_COMPILER_ID:GNU>,$<C_COMPILER_ID:Clang>,$<C_COMPILER_ID:AppleClang>>:-Wno-unused-parameter -Wno-error=unused-parameter>
  $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-Wno-unused-parameter -Wno-error=unused-parameter>
)

if(APPLE)

  # Copied into the bundle's Contents/Frameworks and signed with it.
  target_sources(${CMAKE_PROJECT_NAME} PRIVATE "${ORT_LIBRARY}")
  set_source_files_properties("${ORT_LIBRARY}" PROPERTIES MACOSX_PACKAGE_LOCATION Frameworks)

endif()

if(WIN32)

  # Lives next to the plugin DLL.
  add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ORT_LIBRARY}" "$<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>"
    VERBATIM
  )

  install(FILES "${ORT_LIBRARY}" DESTINATION "${CMAKE_PROJECT_NAME}/bin/64bit")

endif()

#-------------------------------------------------------------------------------
# Deployment
#-------------------------------------------------------------------------------

# Development only: copies the plugin into OBS after each build. The debug
# build scripts turn it on, release builds and CI leave it off.
option(BREEZY_DEPLOY "Copy the plugin into OBS after each build" OFF)

#-------------------------------------------------------------------------------
# Deployment Mac
#-------------------------------------------------------------------------------

if(APPLE AND BREEZY_DEPLOY)

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

if(WIN32 AND BREEZY_DEPLOY)

  set(OBS_PLUGINS_DIR "$ENV{ProgramData}/obs-studio/plugins" CACHE PATH "Destination folder for OBS plugins on Windows")
  set(PLUGIN_DIR "${OBS_PLUGINS_DIR}/${_name}")

  # Copy the built DLL and its data into the plugins dir, and (optionally) restart OBS
  add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Deploying to: ${PLUGIN_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PLUGIN_DIR}/bin/64bit"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${CMAKE_PROJECT_NAME}>" "${PLUGIN_DIR}/bin/64bit/"
    COMMAND ${CMAKE_COMMAND} -E copy "${ORT_LIBRARY}" "${PLUGIN_DIR}/bin/64bit/"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/data" "${PLUGIN_DIR}/data"

    # Optional: brutal restart for dev workflow; comment out if you prefer manual
    # COMMAND ${CMAKE_COMMAND} -E echo "Restarting OBS..."
    # COMMAND cmd /c "taskkill /IM obs64.exe /F 2>nul || exit /b 0"
    # COMMAND cmd /c "start \"\" \"${OBS_EXE}\""
    # VERBATIM
  )
endif()