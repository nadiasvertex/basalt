#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "basalt/tokens.h"
#include "basalt/typography.h"
#include "basalt/units.h"

#include <limits>

using namespace basalt;
using Catch::Matchers::WithinAbs;

TEST_CASE("fixed lengths follow zoom, relative lengths also follow text scale", "[units]") {
  constexpr scale_factors doubled_text{.display = 2, .zoom = 1, .text = 2};
  STATIC_REQUIRE(to_layout(fixed(10), doubled_text) == 10);
  STATIC_REQUIRE(to_layout(relative(10), doubled_text) == 20);

  constexpr scale_factors zoomed{.display = 1, .zoom = 2, .text = 1.5f};
  STATIC_REQUIRE(to_layout(fixed(10), zoomed) == 20);
  STATIC_REQUIRE(to_layout(relative(10), zoomed) == 30);
  STATIC_REQUIRE((relative(4) * 3) == relative(12));
}

TEST_CASE("device conversion uses only the display scale", "[units]") {
  constexpr scale_factors hidpi{.display = 2, .zoom = 1.5f, .text = 2};
  STATIC_REQUIRE(to_device(10.0f, hidpi) == 20);
  STATIC_REQUIRE(from_device(point{20, 40}, hidpi) == point{10, 20});
  STATIC_REQUIRE(to_device(rect{{1, 2}, {3, 4}}, hidpi) == rect{{2, 4}, {6, 8}});
  STATIC_REQUIRE(apply(device_transform(hidpi), {3, 4}) == point{6, 8});
}

TEST_CASE("scale factors must be finite and positive", "[units]") {
  REQUIRE(is_valid({}));
  REQUIRE_FALSE(is_valid({.display = 0}));
  REQUIRE_FALSE(is_valid({.zoom = -1}));
  REQUIRE_FALSE(is_valid({.text = std::numeric_limits<float>::infinity()}));
}

TEST_CASE("snapping lands on device pixel boundaries", "[units]") {
  scale_factors const factors{.display = 1.5f};
  float const snapped = snap_to_device(10.1f, factors);
  REQUIRE_THAT(to_device(snapped, factors), WithinAbs(15, 1e-5));

  auto const r = snap_to_device(rect{{0.2f, 0.4f}, {10.3f, 5.0f}}, factors);
  REQUIRE_THAT(to_device(r.left(), factors), WithinAbs(0, 1e-5));
  REQUIRE_THAT(to_device(r.top(), factors), WithinAbs(1, 1e-5));
  REQUIRE_THAT(to_device(r.right(), factors), WithinAbs(16, 1e-5));
  REQUIRE_THAT(to_device(r.bottom(), factors), WithinAbs(8, 1e-5));
}

TEST_CASE("strokes are at least one device pixel", "[units]") {
  REQUIRE_THAT(device_stroke_width(fixed(0.25f), {.display = 2}), WithinAbs(0.5f, 1e-6));
  REQUIRE_THAT(device_stroke_width(fixed(1), {.display = 2}), WithinAbs(1, 1e-6));
  REQUIRE_THAT(device_stroke_width(fixed(1), {.display = 1, .zoom = 2}), WithinAbs(2, 1e-6));
}

TEST_CASE("size tokens keep their fixed or relative unit", "[units][tokens]") {
  using namespace tokens;
  STATIC_REQUIRE(primitive::size_fixed_12 == fixed(12));
  STATIC_REQUIRE(primitive::size_relative_12 == relative(12));
  STATIC_REQUIRE(primitive::size_4 == fixed(8));
  STATIC_REQUIRE(medium::size_dimension_height_element_min == fixed(32));
  STATIC_REQUIRE(medium_sizes.dimension_height_element_min == fixed(32));
}

TEST_CASE("text styles scale with zoom and text size together", "[units][typography]") {
  constexpr auto body = tokens::text::body_regular_large;
  STATIC_REQUIRE(body.family == "Noto Sans");
  STATIC_REQUIRE(body.font_size == 16);
  STATIC_REQUIRE(body.weight == font_weight::regular);
  STATIC_REQUIRE(body.line_height == 20);

  constexpr auto doubled = to_layout(body, {.zoom = 1, .text = 2});
  STATIC_REQUIRE(doubled.font_size == 32);
  STATIC_REQUIRE(doubled.line_height == 40);
  STATIC_REQUIRE(doubled.family == body.family);
  STATIC_REQUIRE(to_layout(body, {.zoom = 2}).font_size == 32);
}

TEST_CASE("typography tokens resolve their aliases", "[typography][tokens]") {
  using namespace tokens;
  STATIC_REQUIRE(text::heading_xx_large.font_size == primitive::type_size_base_plus_5);
  STATIC_REQUIRE(text::heading_xx_large.weight == primitive::type_weight_bold);
  STATIC_REQUIRE(text::heading_xx_large.letter_spacing ==
                 primitive::type_letter_spacing_base_plus_5);
  STATIC_REQUIRE(text::heading_xx_large.line_height ==
                 primitive::type_line_height_tight_base_plus_5);
  STATIC_REQUIRE(text::body_semibold_medium.weight == font_weight::semibold);
  STATIC_REQUIRE(text_styles.body_semibold_medium == text::body_semibold_medium);
}
