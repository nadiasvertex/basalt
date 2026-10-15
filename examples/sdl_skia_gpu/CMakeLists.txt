# A demo that paints only the damaged part of a Basalt widget tree with Skia, on the platform's
# GPU backend when one can be created and on the CPU otherwise. It is a separate project, like
# tests/consumer, so Basalt's own build and CI never fetch SDL or Skia.
#
#   cmake -S examples/sdl_skia_gpu -B .build/example-gpu -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build .build/example-gpu
#   .build/example-gpu/basalt_sdl_skia_gpu [--software] [--log-damage]
#
# Backends: Metal on macOS, OpenGL on Linux and Windows, CPU raster as the fallback.
# BASALT_CONSUME works as in ../sdl_skia.
cmake_minimum_required(VERSION 3.25)
project(basalt_sdl_skia_gpu LANGUAGES CXX)

if(APPLE)
  # The Skia Metal backend is Objective-C++.
  enable_language(OBJCXX)
  set(CMAKE_OBJCXX_STANDARD 23)
  set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

set(BASALT_CONSUME "subdirectory" CACHE STRING "How to consume Basalt: subdirectory or package")
set(BASALT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." CACHE PATH "Basalt checkout")

if(BASALT_CONSUME STREQUAL "subdirectory")
  add_subdirectory("${BASALT_SOURCE_DIR}" basalt EXCLUDE_FROM_ALL)
  set(BASALT_FONT_DIR "${BASALT_SOURCE_DIR}/tests/data/fonts")
elseif(BASALT_CONSUME STREQUAL "package")
  find_package(basalt 0.1 CONFIG REQUIRED)
  set(BASALT_FONT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/data/fonts"
    CACHE PATH "Directory holding NotoSans-VariableFont_wdth,wght.ttf")
else()
  message(FATAL_ERROR "Unknown BASALT_CONSUME '${BASALT_CONSUME}'.")
endif()

# Builds Skia with the GPU backend of the platform.
set(EXAMPLE_SKIA_GPU ON)
include(example_dependencies)

add_executable(basalt_sdl_skia_gpu
  main.cpp
  damage_renderer.cpp
  raster_target.cpp
  ../common/demo_ui.cpp
  ../common/sdl_events.cpp
  ../common/skia_replay.cpp)
if(APPLE)
  target_sources(basalt_sdl_skia_gpu PRIVATE metal_target.mm)
  set_source_files_properties(metal_target.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
  target_link_libraries(basalt_sdl_skia_gpu PRIVATE
    "-framework Metal" "-framework QuartzCore" "-framework Foundation")
else()
  target_sources(basalt_sdl_skia_gpu PRIVATE gl_target.cpp)
endif()
target_include_directories(basalt_sdl_skia_gpu PRIVATE ../common)
target_link_libraries(basalt_sdl_skia_gpu PRIVATE basalt::basalt SDL3::SDL3 skia::skia)
target_compile_definitions(basalt_sdl_skia_gpu PRIVATE
  BASALT_EXAMPLE_FONT_DIR="${BASALT_FONT_DIR}")
