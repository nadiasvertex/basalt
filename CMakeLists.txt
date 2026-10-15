cmake_minimum_required(VERSION 3.25)
project(basalt VERSION 0.1.0 DESCRIPTION "C++ widget library following the Cubit design system"
  LANGUAGES CXX)


option(BASALT_BUILD_TESTS "Build the Catch2 test suite" ${PROJECT_IS_TOP_LEVEL})
option(BASALT_BUILD_TOOLS "Build the token generator" ${PROJECT_IS_TOP_LEVEL})
option(BASALT_INSTALL "Add install and package export rules" ${PROJECT_IS_TOP_LEVEL})
option(BASALT_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ${PROJECT_IS_TOP_LEVEL})
option(BASALT_SANITIZE "Enable address and undefined-behavior sanitizers" OFF)
set(BASALT_HARFBUZZ "bundled" CACHE STRING
  "Where HarfBuzz comes from: bundled (compiled into basalt) or system (find_package)")
set_property(CACHE BASALT_HARFBUZZ PROPERTY STRINGS bundled system)
if(NOT BASALT_HARFBUZZ MATCHES "^(bundled|system)$")
  message(FATAL_ERROR "BASALT_HARFBUZZ must be 'bundled' or 'system', not '${BASALT_HARFBUZZ}'.")
endif()

if(PROJECT_IS_TOP_LEVEL)
  set(CMAKE_CXX_EXTENSIONS OFF)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
  set(CMAKE_DEBUG_POSTFIX d)
endif()

include(GNUInstallDirs)
include("${PROJECT_SOURCE_DIR}/cmake/compiler_options.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/dependencies.cmake")

add_subdirectory(src)

if(BASALT_BUILD_TOOLS)
  add_subdirectory(tools/basalt_tokens)
endif()

if(BASALT_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(BASALT_INSTALL)
  include("${PROJECT_SOURCE_DIR}/cmake/install.cmake")
endif()
