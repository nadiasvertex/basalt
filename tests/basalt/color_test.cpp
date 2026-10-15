#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "basalt/color.h"
#include "basalt/tokens.h"

using namespace basalt;

TEST_CASE("contrast ratio matches WCAG reference values", "[color]") {
  constexpr rgba black{0, 0, 0};
  constexpr rgba white{255, 255, 255};
  REQUIRE_THAT(contrast_ratio(black, white), Catch::Matchers::WithinAbs(21.0, 0.01));
  REQUIRE_THAT(contrast_ratio(white, black), Catch::Matchers::WithinAbs(21.0, 0.01));
  REQUIRE_THAT(contrast_ratio(white, white), Catch::Matchers::WithinAbs(1.0, 0.001));
}

TEST_CASE("blend_over composites straight alpha", "[color]") {
  auto const mixed = blend_over({0, 0, 0, 128}, {255, 255, 255, 255});
  REQUIRE(mixed.a == 255);
  REQUIRE(mixed.r >= 126);
  REQUIRE(mixed.r <= 128);
}

TEST_CASE("generated tokens resolve aliases and meet AA for body text", "[tokens]") {
  using namespace tokens;
  REQUIRE(light::color_text_default == primitive::color_gray_900);
  REQUIRE(light::color_border_focus == light::color_border_accent);
  REQUIRE(contrast_ratio(light::color_text_default, light::color_background_default) >= 4.5);
  REQUIRE(contrast_ratio(dark::color_text_default, dark::color_background_default) >= 4.5);
  REQUIRE(contrast_ratio(light::color_text_subdued, light::color_background_default) >= 4.5);
}

TEST_CASE("disabled opacity token is exposed", "[tokens]") {
  REQUIRE(tokens::primitive::opacity_disabled == 35.0f);
}

TEST_CASE("with_opacity scales alpha and clamps its factor", "[color]") {
  constexpr rgba red{255, 0, 0, 200};
  REQUIRE(with_opacity(red, 0.5f) == rgba{255, 0, 0, 100});
  REQUIRE(with_opacity(red, 2.0f) == red);
  REQUIRE(with_opacity(red, -1.0f).a == 0);
  STATIC_REQUIRE(opacity_from_percent(35) == 0.35f);
  STATIC_REQUIRE(opacity_from_percent(150) == 1.0f);
}
