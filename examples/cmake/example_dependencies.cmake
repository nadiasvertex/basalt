# SDL 3 and Skia for the example. Provides the targets SDL3::SDL3 and skia::skia.
include_guard(GLOBAL)

include(FetchContent)
include(example_pins)
find_package(Git REQUIRED)

# Declares a dependency pinned by commit. A depth-1 fetch of the exact commit is used because
# GIT_SHALLOW only works for branch tips and Skia's full history is hundreds of megabytes.
function(example_declare_pinned name url commit)
  string(LENGTH "${commit}" length)
  if(NOT commit MATCHES "^[0-9a-f]+$" OR NOT length EQUAL 40)
    message(FATAL_ERROR "${name} must be pinned by a full 40-character commit hash.")
  endif()
  FetchContent_Declare(${name}
    DOWNLOAD_COMMAND "${CMAKE_COMMAND}"
      "-DGIT=${GIT_EXECUTABLE}" "-DURL=${url}" "-DCOMMIT=${commit}"
      "-DSOURCE_DIR=<SOURCE_DIR>"
      -P "${CMAKE_CURRENT_LIST_DIR}/git_fetch_commit.cmake")
endfunction()

# A project that opens no window sets EXAMPLE_WITHOUT_SDL before including this file.
if(NOT EXAMPLE_WITHOUT_SDL)
  find_package(SDL3 3.2 CONFIG QUIET)
endif()
if(NOT EXAMPLE_WITHOUT_SDL AND NOT TARGET SDL3::SDL3)
  message(STATUS "SDL 3 not found; fetching ${EXAMPLE_SDL3_COMMIT}")
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  set(SDL_TESTS OFF CACHE BOOL "" FORCE)
  set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
  example_declare_pinned(SDL3 "${EXAMPLE_SDL3_URL}" "${EXAMPLE_SDL3_COMMIT}")
  FetchContent_MakeAvailable(SDL3)
endif()

include(skia)
