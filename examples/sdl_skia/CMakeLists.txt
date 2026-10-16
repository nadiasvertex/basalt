# A demo that draws a Basalt widget tree with Skia into an SDL window. It is a separate project,
# like tests/consumer, so Basalt's own build and CI never fetch SDL or Skia.
#
#   cmake -S examples/sdl_skia -B .build/example -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build .build/example
#   .build/example/basalt_sdl_skia
#
# BASALT_CONSUME picks how Basalt is found:
#   subdirectory  add_subdirectory(BASALT_SOURCE_DIR), the default
#   package       find_package(basalt) from an install prefix (set CMAKE_PREFIX_PATH)
cmake_minimum_required(VERSION 3.25)
project(basalt_sdl_skia LANGUAGES CXX)

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

include(example_dependencies)

add_executable(basalt_sdl_skia
  main.cpp
  ../common/demo_ui.cpp
  ../common/sdl_events.cpp
  ../common/skia_painter.cpp
  ../common/skia_replay.cpp)
target_include_directories(basalt_sdl_skia PRIVATE ../common)
target_link_libraries(basalt_sdl_skia PRIVATE basalt::basalt SDL3::SDL3 skia::skia)
target_compile_definitions(basalt_sdl_skia PRIVATE
  BASALT_EXAMPLE_FONT_DIR="${BASALT_FONT_DIR}")
