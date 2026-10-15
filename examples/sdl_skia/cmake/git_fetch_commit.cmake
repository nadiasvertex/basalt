# Populates SOURCE_DIR with exactly COMMIT from URL using a depth-1 fetch by
# hash. Run as: cmake -DGIT=... -DURL=... -DCOMMIT=... -DSOURCE_DIR=... -P <this>
foreach(var GIT URL COMMIT SOURCE_DIR)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "git_fetch_commit.cmake requires -D${var}=...")
  endif()
endforeach()

function(run_git)
  execute_process(COMMAND "${GIT}" ${ARGN}
    WORKING_DIRECTORY "${SOURCE_DIR}"
    RESULT_VARIABLE result OUTPUT_QUIET)
  if(NOT result EQUAL 0)
    string(JOIN " " args ${ARGN})
    message(FATAL_ERROR "git ${args} failed in ${SOURCE_DIR} (${result}).")
  endif()
endfunction()

if(IS_DIRECTORY "${SOURCE_DIR}/.git")
  execute_process(COMMAND "${GIT}" rev-parse HEAD
    WORKING_DIRECTORY "${SOURCE_DIR}"
    OUTPUT_VARIABLE head OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  if(head STREQUAL COMMIT)
    return()
  endif()
endif()

file(REMOVE_RECURSE "${SOURCE_DIR}")
file(MAKE_DIRECTORY "${SOURCE_DIR}")
run_git(init --quiet)
run_git(remote add origin "${URL}")
run_git(fetch --quiet --depth 1 origin "${COMMIT}")
run_git(-c advice.detachedHead=false checkout --quiet FETCH_HEAD)
