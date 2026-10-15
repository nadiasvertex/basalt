# Fails when Basalt reaches outside itself, or when the umbrella header is incomplete:
#   - a quoted include with a directory must be a basalt/ header;
#   - basalt/basalt.h must include every other public header.
# Usage: cmake -DBASALT_SOURCE_DIR=<repo> -P <this>
cmake_minimum_required(VERSION 3.25)

set(_errors "")
set(_include_regex "^[ \t]*#[ \t]*include[ \t]*\"([^\"]+)\"")

file(GLOB_RECURSE _files
  "${BASALT_SOURCE_DIR}/include/*" "${BASALT_SOURCE_DIR}/generated/*"
  "${BASALT_SOURCE_DIR}/src/*")
foreach(_file IN LISTS _files)
  if(NOT _file MATCHES "\\.(h|hpp|cpp)$")
    continue()
  endif()
  file(RELATIVE_PATH _rel "${BASALT_SOURCE_DIR}" "${_file}")
  file(STRINGS "${_file}" _lines REGEX "${_include_regex}")
  foreach(_line IN LISTS _lines)
    string(REGEX REPLACE "${_include_regex}.*" "\\1" _include "${_line}")
    if(_include MATCHES "/" AND NOT _include MATCHES "^basalt/")
      list(APPEND _errors "${_rel}: may not include \"${_include}\"")
    endif()
    if(_include STREQUAL "basalt/basalt.h")
      list(APPEND _errors "${_rel}: may not include the umbrella header")
    endif()
  endforeach()
endforeach()

file(GLOB_RECURSE _public RELATIVE "${BASALT_SOURCE_DIR}/include" "${BASALT_SOURCE_DIR}/include/*.h")
file(GLOB_RECURSE _generated RELATIVE "${BASALT_SOURCE_DIR}/generated"
  "${BASALT_SOURCE_DIR}/generated/*.h")
list(APPEND _public ${_generated})
file(STRINGS "${BASALT_SOURCE_DIR}/include/basalt/basalt.h" _umbrella_lines REGEX "${_include_regex}")
set(_umbrella "")
foreach(_line IN LISTS _umbrella_lines)
  string(REGEX REPLACE "${_include_regex}.*" "\\1" _include "${_line}")
  list(APPEND _umbrella "${_include}")
endforeach()
foreach(_header IN LISTS _public)
  if(NOT _header STREQUAL "basalt/basalt.h" AND NOT _header IN_LIST _umbrella)
    list(APPEND _errors "include/basalt/basalt.h: does not include \"${_header}\"")
  endif()
endforeach()

if(_errors)
  list(JOIN _errors "\n  " _message)
  message(FATAL_ERROR "Include boundary violations:\n  ${_message}")
endif()
message(STATUS "Include boundaries OK")
