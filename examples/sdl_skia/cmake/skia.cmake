# Skia only supports GN and Ninja, so this file populates a pinned checkout, runs `gn gen` at
# configure time, asks GN for the libraries, frameworks and defines a consumer needs, and builds
# them with Ninja from one custom command. The result is the imported target skia::skia.
#
# The example renders on the CPU, so the GPU backends, codecs, PDF, SVG and Skia's own shaper are
# all disabled. Basalt shapes text with HarfBuzz and hands Skia glyph ids.
include_guard(GLOBAL)

include(example_pins)
find_package(SkiaTools REQUIRED)

get_property(_skia_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(_skia_multi_config)
  message(FATAL_ERROR
    "The Skia adapter needs a single-config generator (e.g. Ninja) so GN arguments match "
    "CMAKE_BUILD_TYPE.")
endif()

example_declare_pinned(skia "${EXAMPLE_SKIA_URL}" "${EXAMPLE_SKIA_COMMIT}")
# Skia has no CMakeLists.txt, so this only populates the source directory.
FetchContent_MakeAvailable(skia)
set(_skia_src "${skia_SOURCE_DIR}")
set(_skia_gn "${_skia_src}/bin/gn")
if(CMAKE_HOST_WIN32)
  set(_skia_gn "${_skia_src}/bin/gn.exe")
endif()

# Third-party checkouts that the configuration below compiles in. Update this list when the GN
# arguments or the pin change; a missing entry fails at `gn gen` or while building Skia.
if(APPLE OR WIN32)
  set(_skia_externals "")
else()
  set(_skia_externals freetype libpng zlib)
endif()

set(_skia_sync_stamp "${CMAKE_BINARY_DIR}/skia/sync.stamp")
set(_skia_sync_key "${EXAMPLE_SKIA_COMMIT};${_skia_externals}")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/skia")
set(_skia_synced_key "")
if(EXISTS "${_skia_sync_stamp}")
  file(READ "${_skia_sync_stamp}" _skia_synced_key)
endif()
if(NOT _skia_synced_key STREQUAL _skia_sync_key)
  message(STATUS "Skia: syncing GN and third-party checkouts")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
      "GIT_EXECUTABLE=${SKIA_GIT_EXECUTABLE}" GIT_SYNC_DEPS_QUIET=1
      "${SKIA_PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/skia_sync_deps.py"
      "${_skia_src}" "${CMAKE_BINARY_DIR}/skia/DEPS" ${_skia_externals}
    RESULT_VARIABLE _skia_result)
  if(NOT _skia_result EQUAL 0)
    message(FATAL_ERROR "Skia: syncing third-party checkouts failed (${_skia_result}).")
  endif()
  execute_process(
    COMMAND "${SKIA_PYTHON_EXECUTABLE}" bin/fetch-gn
    WORKING_DIRECTORY "${_skia_src}"
    RESULT_VARIABLE _skia_result)
  if(NOT _skia_result EQUAL 0)
    message(FATAL_ERROR "Skia: bin/fetch-gn failed (${_skia_result}).")
  endif()
  file(WRITE "${_skia_sync_stamp}" "${_skia_sync_key}")
endif()

# Per-configuration output so Debug and Release builds can coexist.
if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR NOT CMAKE_BUILD_TYPE)
  set(_skia_config debug)
  # Official builds forbid is_debug, so Debug uses a developer build.
  set(_skia_args "is_official_build=false" "is_debug=true")
else()
  set(_skia_config release)
  set(_skia_args "is_official_build=true" "is_debug=false")
endif()
set(_skia_out "${CMAKE_BINARY_DIR}/skia/${_skia_config}")

list(APPEND _skia_args
  "is_component_build=false"
  "skia_enable_android_utils=false"
  "skia_enable_gpu=false"
  "skia_enable_gpu_debug_layers=false"
  "skia_enable_spirv_validation=false"
  "skia_enable_pdf=false"
  "skia_enable_skottie=false"
  "skia_enable_skshaper=false"
  "skia_enable_skunicode=false"
  "skia_enable_svg=false"
  "skia_enable_tools=false"
  "skia_use_dng_sdk=false"
  "skia_use_expat=false"
  "skia_use_harfbuzz=false"
  "skia_use_icu=false"
  "skia_use_jpeg_gainmaps=false"
  "skia_use_libavif=false"
  "skia_use_libheif=false"
  "skia_use_libjpeg_turbo_decode=false"
  "skia_use_libjpeg_turbo_encode=false"
  "skia_use_libjxl_decode=false"
  "skia_use_libpng_decode=false"
  "skia_use_libpng_encode=false"
  "skia_use_libwebp_decode=false"
  "skia_use_libwebp_encode=false"
  "skia_use_lua=false"
  "skia_use_piex=false"
  "skia_use_wuffs=false"
  "skia_use_xps=false"
  "skia_use_gl=false"
  "skia_use_metal=false"
  "skia_use_direct3d=false"
  "skia_use_vulkan=false")

# SkWriteBuffer otherwise needs the disabled PNG encoder.
set(_skia_cflags "-DSK_DISABLE_LEGACY_PNG_WRITEBUFFER")

if(APPLE OR WIN32)
  list(APPEND _skia_args "skia_use_fontconfig=false" "skia_use_freetype=false")
elseif(UNIX)
  list(APPEND _skia_args
    "skia_use_x11=false"
    "skia_use_fontconfig=true" "skia_use_freetype=true"
    "skia_use_system_freetype2=false" "skia_use_system_libpng=false"
    "skia_use_system_zlib=false")
else()
  message(FATAL_ERROR "Skia is not supported on ${CMAKE_SYSTEM_NAME}.")
endif()

if(WIN32)
  # GN leaves MSVC on its static runtime; match CMake's DLL runtime instead.
  if(_skia_config STREQUAL "debug")
    set(_skia_cflags "/MDd" "/DSK_DISABLE_LEGACY_PNG_WRITEBUFFER")
  else()
    set(_skia_cflags "/MD" "/DSK_DISABLE_LEGACY_PNG_WRITEBUFFER")
  endif()
  if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    cmake_path(GET CMAKE_CXX_COMPILER PARENT_PATH _skia_clang_bin)
    cmake_path(GET _skia_clang_bin PARENT_PATH _skia_clang_root)
    list(APPEND _skia_args "clang_win=\"${_skia_clang_root}\"")
  endif()
else()
  # Build Skia with the project's compiler so the C++ ABI matches.
  list(APPEND _skia_args "cxx=\"${CMAKE_CXX_COMPILER}\"")
  if(CMAKE_C_COMPILER)
    list(APPEND _skia_args "cc=\"${CMAKE_C_COMPILER}\"")
  else()
    # The project enables only CXX; the sibling C driver is the same compiler.
    string(REGEX REPLACE "\\+\\+(-[0-9.]+)?$" "\\1" _skia_cc "${CMAKE_CXX_COMPILER}")
    list(APPEND _skia_args "cc=\"${_skia_cc}\"")
  endif()
endif()
if(CMAKE_OSX_ARCHITECTURES MATCHES ";")
  message(FATAL_ERROR "The Skia adapter does not support universal builds.")
elseif(CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64")
  list(APPEND _skia_args "target_cpu=\"x64\"")
elseif(CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
  list(APPEND _skia_args "target_cpu=\"arm64\"")
endif()

list(TRANSFORM _skia_cflags PREPEND "\"")
list(TRANSFORM _skia_cflags APPEND "\"")
list(JOIN _skia_cflags ", " _skia_cflags)
list(APPEND _skia_args "extra_cflags=[${_skia_cflags}]")
list(SORT _skia_args)

# The pin is recorded so a version bump regenerates args.gn and rebuilds Skia.
string(REPLACE ";" "\n" _skia_args_text "${_skia_args}")
file(MAKE_DIRECTORY "${_skia_out}")
set(_skia_args_file "${_skia_out}/args.gn")
file(CONFIGURE OUTPUT "${_skia_args_file}" CONTENT
  "# skia ${EXAMPLE_SKIA_COMMIT}\n${_skia_args_text}\n" @ONLY NEWLINE_STYLE UNIX)

if(NOT EXISTS "${_skia_out}/build.ninja"
    OR "${_skia_args_file}" IS_NEWER_THAN "${_skia_out}/build.ninja")
  execute_process(
    COMMAND "${_skia_gn}" gen "${_skia_out}" --fail-on-unused-args
    WORKING_DIRECTORY "${_skia_src}"
    RESULT_VARIABLE _skia_result
    OUTPUT_VARIABLE _skia_output
    ERROR_VARIABLE _skia_output)
  if(NOT _skia_result EQUAL 0)
    message(FATAL_ERROR "Skia: gn gen failed:\n${_skia_output}")
  endif()
endif()

# Asks GN about a label and returns one list entry per output line.
function(_example_skia_query out_var label what)
  execute_process(
    COMMAND "${_skia_gn}" desc "${_skia_out}" "${label}" ${what} ${ARGN}
    WORKING_DIRECTORY "${_skia_src}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "Skia: gn desc ${label} ${what} failed:\n${output}${error}")
  endif()
  string(STRIP "${output}" output)
  string(REPLACE "\n" ";" output "${output}")
  set(${out_var} "${output}" PARENT_SCOPE)
endfunction()

_example_skia_query(_skia_static_deps //:skia deps --all --type=static_library --as=output)
_example_skia_query(_skia_defines //:skia_public defines)
_example_skia_query(_skia_system_libs //:skia libs)
_example_skia_query(_skia_frameworks //:skia frameworks)

set(_skia_main "libskia.a")
if(WIN32)
  set(_skia_main "skia.lib")
endif()
set(_skia_outputs "${_skia_out}/${_skia_main}")
foreach(_lib IN LISTS _skia_static_deps)
  list(APPEND _skia_outputs "${_skia_out}/${_lib}")
endforeach()

# The libraries are declared OUTPUTS, so application edits never re-enter Skia's build and
# Skia's own build.ninja handles incremental rebuilds inside it.
add_custom_command(
  OUTPUT ${_skia_outputs}
  COMMAND "${SKIA_NINJA_EXECUTABLE}" -C "${_skia_out}" skia
  DEPENDS "${_skia_args_file}" "${_skia_sync_stamp}"
  WORKING_DIRECTORY "${_skia_src}"
  COMMENT "Skia: building ${_skia_config}"
  USES_TERMINAL
  VERBATIM)
add_custom_target(skia_build DEPENDS ${_skia_outputs})

# skia comes first so static linkers resolve its references to the other archives.
add_library(skia_interface INTERFACE)
add_dependencies(skia_interface skia_build)
target_include_directories(skia_interface SYSTEM INTERFACE "${_skia_src}")
target_compile_definitions(skia_interface INTERFACE ${_skia_defines})
set(_skia_index 0)
foreach(_path IN LISTS _skia_outputs)
  add_library(skia_imported_${_skia_index} STATIC IMPORTED)
  set_target_properties(skia_imported_${_skia_index} PROPERTIES IMPORTED_LOCATION "${_path}")
  target_link_libraries(skia_interface INTERFACE skia_imported_${_skia_index})
  math(EXPR _skia_index "${_skia_index} + 1")
endforeach()
target_link_libraries(skia_interface INTERFACE ${_skia_system_libs})
foreach(_framework IN LISTS _skia_frameworks)
  string(REGEX REPLACE "\\.framework$" "" _framework "${_framework}")
  target_link_libraries(skia_interface INTERFACE "-framework ${_framework}")
endforeach()
add_library(skia::skia ALIAS skia_interface)
