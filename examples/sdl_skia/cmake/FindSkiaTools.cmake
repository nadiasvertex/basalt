# Locates the host tools needed to build Skia through GN/Ninja.
#
# Result variables:
#   SkiaTools_FOUND        - all required tools were found
#   SKIA_PYTHON_EXECUTABLE - Python 3 interpreter (runs git-sync-deps, fetch-gn)
#   SKIA_NINJA_EXECUTABLE  - Ninja
#   SKIA_GIT_EXECUTABLE    - Git (used by tools/git-sync-deps)
#
# GN is not searched for here: it is downloaded by Skia's bin/fetch-gn into the
# pinned checkout so its version always matches the Skia pin.
include_guard(GLOBAL)

find_package(Python3 3 QUIET COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  set(SKIA_PYTHON_EXECUTABLE "${Python3_EXECUTABLE}")
endif()
find_program(SKIA_NINJA_EXECUTABLE NAMES ninja ninja-build)
find_program(SKIA_GIT_EXECUTABLE NAMES git)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SkiaTools
  REQUIRED_VARS SKIA_PYTHON_EXECUTABLE SKIA_NINJA_EXECUTABLE SKIA_GIT_EXECUTABLE)
mark_as_advanced(SKIA_NINJA_EXECUTABLE SKIA_GIT_EXECUTABLE)
