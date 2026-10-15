if(BASALT_SANITIZE)
  message(FATAL_ERROR "Benchmarks measure the cost of instrumentation under BASALT_SANITIZE. "
    "Configure without it.")
endif()
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE MATCHES "^(Release|RelWithDebInfo)$")
  message(WARNING "Benchmarks need an optimized build. CMAKE_BUILD_TYPE is "
    "'${CMAKE_BUILD_TYPE}'; use the benchmark preset.")
endif()

add_executable(basalt_benchmarks widgets_benchmark.cpp)
target_include_directories(basalt_benchmarks PRIVATE ${PROJECT_SOURCE_DIR}/tests/basalt)
target_link_libraries(basalt_benchmarks PRIVATE basalt::basalt Catch2::Catch2WithMain
  basalt_compiler_options)
target_compile_definitions(basalt_benchmarks PRIVATE
  BASALT_TEST_FONT_DIR="${PROJECT_SOURCE_DIR}/tests/data/fonts")
