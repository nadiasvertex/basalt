# A minimal project that uses Basalt the way another project would. BASALT_CONSUME picks how:
#   package       find_package(basalt) from an install prefix (set CMAKE_PREFIX_PATH)
#   subdirectory  add_subdirectory(BASALT_SOURCE_DIR)
#   fetch         FetchContent from BASALT_GIT_REPOSITORY at BASALT_GIT_TAG
cmake_minimum_required(VERSION 3.25)
project(basalt_consumer LANGUAGES CXX)

set(BASALT_CONSUME "package" CACHE STRING "How to consume Basalt: package, subdirectory or fetch")
set(BASALT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." CACHE PATH "Basalt checkout")
set(BASALT_GIT_REPOSITORY "https://github.com/OWNER/basalt.git" CACHE STRING "Basalt repository")
set(BASALT_GIT_TAG "main" CACHE STRING "Basalt tag or commit")

if(BASALT_CONSUME STREQUAL "package")
  find_package(basalt 0.1 CONFIG REQUIRED)
elseif(BASALT_CONSUME STREQUAL "subdirectory")
  add_subdirectory("${BASALT_SOURCE_DIR}" basalt)
elseif(BASALT_CONSUME STREQUAL "fetch")
  include(FetchContent)
  FetchContent_Declare(basalt GIT_REPOSITORY "${BASALT_GIT_REPOSITORY}" GIT_TAG "${BASALT_GIT_TAG}")
  FetchContent_MakeAvailable(basalt)
else()
  message(FATAL_ERROR "Unknown BASALT_CONSUME '${BASALT_CONSUME}'.")
endif()

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE basalt::basalt)

enable_testing()
add_test(NAME consumer
  COMMAND consumer "${CMAKE_CURRENT_SOURCE_DIR}/../data/fonts/NotoSans-VariableFont_wdth,wght.ttf")
