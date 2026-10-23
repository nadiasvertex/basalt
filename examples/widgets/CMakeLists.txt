# One minimal program per widget, built without a window. Each program builds a widget, simulates
# input and prints what the handlers received. With a file name as its first argument, it also
# writes a screenshot of the widget. The documentation in docs/widgets/ refers to these programs.
#
#   cmake -S examples/widgets -B .build/widgets -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build .build/widgets
#   .build/widgets/button
#   .build/widgets/button button.pam
#
# BASALT_CONSUME picks how Basalt is found:
#   subdirectory  add_subdirectory(BASALT_SOURCE_DIR), the default
#   package       find_package(basalt) from an install prefix (set CMAKE_PREFIX_PATH)
cmake_minimum_required(VERSION 3.25)
project(basalt_widget_examples LANGUAGES CXX)

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

set(EXAMPLE_WITHOUT_SDL ON)
include(example_dependencies)

add_library(example_host STATIC
  common/example_host.cpp
  ../common/skia_painter.cpp
  ../common/skia_replay.cpp)
target_include_directories(example_host PUBLIC common PRIVATE ../common)
target_link_libraries(example_host PUBLIC basalt::basalt PRIVATE skia::skia)
target_compile_definitions(example_host PRIVATE BASALT_EXAMPLE_FONT_DIR="${BASALT_FONT_DIR}")

# Each example is the file <name>.cpp and the program <name>.
set(WIDGET_EXAMPLES
  accordion
  avatar
  box
  button
  checkbox
  chip
  collection_view
  diagnostics_list
  dialog
  grid_view
  image_tile
  image_view
  label
  menu
  menu_bar
  navigation_bar
  overlay_host
  palette
  popover
  progress_bar
  radio_button
  scroll_bar
  scroll_view
  select
  separator
  slider
  split_view
  stack
  status_bar
  tabs
  text_field
  toggle
  tooltip
  tree_view
  uploader
)

# The tests are smoke tests. A program must start, build its widget and exit with 0.
enable_testing()
foreach(name IN LISTS WIDGET_EXAMPLES)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE example_host)
  add_test(NAME example_${name} COMMAND ${name})
endforeach()
