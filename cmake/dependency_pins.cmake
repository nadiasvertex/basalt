# Every fetched dependency, pinned to a release archive and its SHA-256. Updating a
# dependency is a two-line review here. Set FETCHCONTENT_SOURCE_DIR_<NAME> to use a
# local checkout instead, for example in offline builds.
include_guard(GLOBAL)

set(BASALT_HARFBUZZ_VERSION "11.4.4")
set(BASALT_HARFBUZZ_URL
  "https://github.com/harfbuzz/harfbuzz/releases/download/${BASALT_HARFBUZZ_VERSION}/harfbuzz-${BASALT_HARFBUZZ_VERSION}.tar.xz")
set(BASALT_HARFBUZZ_SHA256 "1053f17146ea587fafa9f56c9e4e0f4b3fefae4ba2569155d8bc9a35208f58c5")

set(BASALT_CATCH2_VERSION "3.16.0")
set(BASALT_CATCH2_URL
  "https://github.com/catchorg/Catch2/archive/refs/tags/v${BASALT_CATCH2_VERSION}.tar.gz")
set(BASALT_CATCH2_SHA256 "0957cae5821b17ce07f0833aaa52b5137643a8382203221f363a8303c109af34")
