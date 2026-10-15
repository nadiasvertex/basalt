# Warning policy and sanitizer wiring for Basalt's own targets. Linked PRIVATE, so none of it
# reaches consumers.
add_library(basalt_compiler_options INTERFACE)

if(MSVC)
  target_compile_options(basalt_compiler_options INTERFACE /W4 /utf-8)
  if(BASALT_WARNINGS_AS_ERRORS)
    target_compile_options(basalt_compiler_options INTERFACE /WX)
  endif()
else()
  target_compile_options(basalt_compiler_options INTERFACE -Wall)
  if(BASALT_WARNINGS_AS_ERRORS)
    target_compile_options(basalt_compiler_options INTERFACE -Werror)
  endif()
endif()

if(BASALT_SANITIZE AND NOT MSVC)
  target_compile_options(basalt_compiler_options INTERFACE
    -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(basalt_compiler_options INTERFACE -fsanitize=address,undefined)
endif()
