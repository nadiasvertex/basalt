#include <catch2/catch_test_macros.hpp>

#include "basalt/widgets/hover_delay.h"

using namespace basalt;
using namespace std::chrono_literals;

TEST_CASE("a hover delay fires once when the time has passed", "[hover_delay]") {
  hover_delay delay{100ms};
  delay.start();
  REQUIRE(delay.pending());
  REQUIRE_FALSE(delay.advance(60ms));
  REQUIRE(delay.pending());
  REQUIRE(delay.advance(60ms));
  REQUIRE_FALSE(delay.pending());
  REQUIRE_FALSE(delay.advance(60ms));
}

TEST_CASE("an idle or cancelled hover delay never fires", "[hover_delay]") {
  hover_delay delay{100ms};
  REQUIRE_FALSE(delay.advance(1s));
  delay.start();
  delay.cancel();
  REQUIRE_FALSE(delay.advance(1s));
}

TEST_CASE("starting again restarts the countdown", "[hover_delay]") {
  hover_delay delay{100ms};
  delay.start();
  REQUIRE_FALSE(delay.advance(80ms));
  delay.start();
  REQUIRE_FALSE(delay.advance(80ms));
  REQUIRE(delay.advance(20ms));
}
