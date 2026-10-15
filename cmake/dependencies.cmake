# Third-party dependencies: HarfBuzz for shaping, and Catch2 for the tests.
include(FetchContent)
include("${CMAKE_CURRENT_LIST_DIR}/dependency_pins.cmake")

if(BASALT_HARFBUZZ STREQUAL "bundled")
  # Only the sources are needed: src/harfbuzz.cc is HarfBuzz's single-file build, compiled into
  # basalt with every optional backend off. SOURCE_SUBDIR names a directory that does not exist,
  # so FetchContent populates the sources without running HarfBuzz's own CMake project.
  FetchContent_Declare(harfbuzz
    URL "${BASALT_HARFBUZZ_URL}"
    URL_HASH "SHA256=${BASALT_HARFBUZZ_SHA256}"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR basalt-sources-only)
  FetchContent_MakeAvailable(harfbuzz)

  add_library(basalt_harfbuzz OBJECT "${harfbuzz_SOURCE_DIR}/src/harfbuzz.cc")
  target_include_directories(basalt_harfbuzz SYSTEM PUBLIC "${harfbuzz_SOURCE_DIR}/src")
  set_target_properties(basalt_harfbuzz PROPERTIES POSITION_INDEPENDENT_CODE ON)
  if(MSVC)
    target_compile_options(basalt_harfbuzz PRIVATE /w /utf-8)
  else()
    target_compile_options(basalt_harfbuzz PRIVATE -w)
  endif()
else()
  find_package(harfbuzz CONFIG QUIET)
  if(NOT TARGET harfbuzz::harfbuzz)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(harfbuzz REQUIRED IMPORTED_TARGET harfbuzz)
    add_library(harfbuzz::harfbuzz ALIAS PkgConfig::harfbuzz)
  endif()
endif()

if(BASALT_BUILD_TESTS)
  # An installed Catch2 3 is used when one is found; otherwise the pinned release is fetched.
  FetchContent_Declare(Catch2
    URL "${BASALT_CATCH2_URL}"
    URL_HASH "SHA256=${BASALT_CATCH2_SHA256}"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    FIND_PACKAGE_ARGS 3)
  # Catch2 enables its std::string_view support from the language level it is compiled with,
  # which must match the tests.
  block(SCOPE_FOR VARIABLES PROPAGATE catch2_SOURCE_DIR Catch2_DIR)
    set(CMAKE_CXX_STANDARD 23)
    FetchContent_MakeAvailable(Catch2)
  endblock()
  # Catch.cmake provides catch_discover_tests.
  if(catch2_SOURCE_DIR)
    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
  elseif(Catch2_DIR)
    list(APPEND CMAKE_MODULE_PATH "${Catch2_DIR}")
  endif()
  include(Catch)
endif()
