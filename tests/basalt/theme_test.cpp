#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "basalt/color.h"
#include "basalt/theme.h"

using namespace basalt;
using Catch::Matchers::WithinAbs;

TEST_CASE("a value-initialized theme is light at medium density", "[theme]") {
  REQUIRE(theme{} == make_theme(color_scheme::light));
  REQUIRE(theme{}.colors == tokens::light_colors);
  REQUIRE(theme{}.sizes == tokens::medium_sizes);
}

TEST_CASE("make_theme selects colors by scheme and sizes by density", "[theme]") {
  auto const dark_small = make_theme(color_scheme::dark, density::small);
  REQUIRE(dark_small.scheme == color_scheme::dark);
  REQUIRE(dark_small.density == density::small);
  REQUIRE(dark_small.colors.text_default == tokens::dark::color_text_default);
  REQUIRE(dark_small.sizes.dimension_height_element_min ==
          tokens::small::size_dimension_height_element_min);

  auto const light_large = make_theme(color_scheme::light, density::large);
  REQUIRE(light_large.colors == tokens::light_colors);
  REQUIRE(light_large.sizes == tokens::large_sizes);
  REQUIRE(light_large.colors != dark_small.colors);
}

TEST_CASE("theme text styles cover UI, reading and source text", "[theme][typography]") {
  auto const t = make_theme(color_scheme::light);
  REQUIRE(t.text == tokens::text_styles);
  REQUIRE(t.reading_text.family == tokens::primitive::type_family_ui);
  REQUIRE(t.reading_text.line_height == tokens::primitive::type_line_height_loose_base);
  REQUIRE(t.reading_text.font_size == tokens::primitive::type_size_base);
  REQUIRE(t.source_text.family == monospace_family);
  REQUIRE(t.source_text.font_size == tokens::text::body_regular_large.font_size);
}

TEST_CASE("disabled opacity comes from the opacity token", "[theme][color]") {
  auto const t = make_theme(color_scheme::dark);
  REQUIRE_THAT(t.disabled_opacity, WithinAbs(0.35, 1e-6));
  REQUIRE(with_opacity(t.colors.text_default, t.disabled_opacity).a == 89);
}

TEST_CASE("large density meets the Cubit 40px minimum target size", "[theme]") {
  auto const t = make_theme(color_scheme::light, density::large);
  REQUIRE(t.sizes.dimension_height_element_min.value >= 40);
  REQUIRE(t.sizes.dimension_width_element_min.value >= 40);
}

TEST_CASE("both schemes keep body text at WCAG AA contrast", "[theme][color]") {
  for (auto const scheme : {color_scheme::light, color_scheme::dark}) {
    auto const &c = colors_for(scheme);
    REQUIRE(contrast_ratio(c.text_default, c.background_default) >= 4.5);
    REQUIRE(contrast_ratio(c.text_subdued, c.background_default) >= 4.5);
  }
}
