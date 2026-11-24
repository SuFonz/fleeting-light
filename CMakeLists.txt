cmake_minimum_required(VERSION 3.20)

project(
  fleeting_light
  VERSION 0.1.0
  DESCRIPTION "Fleeting Light - WebView dynamic wallpaper"
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

option(FL_STATIC_RUNTIME "Static link C++/GCC runtime when supported" ON)

# ---------------------------------------------------------------------------
# Core layer (shared across platforms)
# ---------------------------------------------------------------------------
add_library(
  fl_core STATIC
  src/core/app.cpp
  src/core/config.cpp
  src/core/json.cpp
  src/core/log.cpp)
target_include_directories(fl_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_definitions(fl_core PUBLIC FL_VERSION="${PROJECT_VERSION}")

# ---------------------------------------------------------------------------
# Platform layer: per-OS dispatch (add src/platform/linux/ later)
# ---------------------------------------------------------------------------
if(WIN32)
  set(FL_WEBVIEW2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/webview2")
  set(FL_WEBVIEW2_VERSION "1.0.4370.26" CACHE STRING "WebView2 NuGet version")
  set(FL_WEBVIEW2_LOCAL_PACKAGE "" CACHE FILEPATH "Local WebView2 .nupkg for offline build")

  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/WebView2Sdk.cmake")
  fl_ensure_webview2_sdk()

  add_library(fl_webview2 INTERFACE)
  target_include_directories(fl_webview2 SYSTEM INTERFACE "${FL_WEBVIEW2_DIR}/include")
  target_compile_definitions(fl_webview2 INTERFACE UNICODE _UNICODE)
  target_link_libraries(
    fl_webview2
    INTERFACE ole32 oleaut32 uuid user32 gdi32 shell32 shlwapi dwmapi wtsapi32 advapi32)

  add_library(
    fl_platform STATIC
    src/platform/win/autostart.cpp
    src/platform/win/desktop_host.cpp
    src/platform/win/env_win.cpp
    src/platform/win/tray.cpp
    src/platform/win/wallpaper_picker.cpp
    src/platform/win/webview2_env.cpp
    src/platform/win/webview2_loader.cpp
    src/platform/win/webview2_surface.cpp
    src/platform/win/win_app.cpp
    src/platform/win/win_util.cpp)
  target_link_libraries(fl_platform PUBLIC fl_core fl_webview2)

  add_executable(fleeting_light WIN32 src/main.cpp)
  target_link_options(fleeting_light PRIVATE -municode)

  # application icon: needs windres (RC language). Skipped gracefully if unavailable.
  include(CheckLanguage)
  check_language(RC)
  if(CMAKE_RC_COMPILER)
    enable_language(RC)
    target_sources(fleeting_light PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc")
  else()
    message(WARNING "windres not found: building without the application icon")
  endif()

  add_custom_command(
    TARGET fleeting_light
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${FL_WEBVIEW2_DIR}/bin/x64/WebView2Loader.dll"
            "$<TARGET_FILE_DIR:fleeting_light>/WebView2Loader.dll"
    COMMENT "Copying WebView2Loader.dll")
elseif(UNIX AND NOT APPLE)
  # placeholder: WebKitGTK / Qt WebEngine / LayerShell backend goes here
  message(FATAL_ERROR "Linux backend is not implemented yet. Reuse fl_core and web/ when adding src/platform/linux.")
else()
  message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

target_link_libraries(fleeting_light PRIVATE fl_platform)

# ---------------------------------------------------------------------------
# Common compile / link options (GNU / Clang / llvm-mingw)
# ---------------------------------------------------------------------------
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(fl_core PRIVATE -Wall -Wextra -Wno-unused-parameter)
  target_compile_options(fl_platform PRIVATE -Wall -Wextra -Wno-unused-parameter)
  target_compile_options(fleeting_light PRIVATE -Wall -Wextra)

  if(FL_STATIC_RUNTIME)
    if(MINGW)
      target_link_options(fleeting_light PRIVATE -static)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND UNIX AND NOT APPLE)
      # Linux clang: dynamic runtime is the safer default; switch to static if needed
      target_link_options(fleeting_light PRIVATE -static-libgcc -static-libstdc++)
    endif()
  endif()
endif()

# ---------------------------------------------------------------------------
# Frontend asset sync + dev run target
# ---------------------------------------------------------------------------
add_custom_target(
  fl_web_assets ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/web"
          "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/web"
  COMMENT "Syncing web assets -> bin/web")
add_dependencies(fleeting_light fl_web_assets)

add_custom_target(
  run
  COMMAND ${CMAKE_COMMAND} -E env
          "FL_WEB_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/web"
          "FL_DEV=1"
          "$<TARGET_FILE:fleeting_light>" --dev
  WORKING_DIRECTORY "$<TARGET_FILE_DIR:fleeting_light>"
  DEPENDS fleeting_light
  USES_TERMINAL)