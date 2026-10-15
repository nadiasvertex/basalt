#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "basalt/geometry.h"

#include <numbers>

using namespace basalt;
using Catch::Matchers::WithinAbs;

TEST_CASE("rect edges, containment and emptiness", "[geometry]") {
  constexpr rect r{{10, 20}, {30, 40}};
  STATIC_REQUIRE(r.right() == 40);
  STATIC_REQUIRE(r.bottom() == 60);
  STATIC_REQUIRE(r.center() == point{25, 40});
  STATIC_REQUIRE(r.contains({10, 20}));
  STATIC_REQUIRE_FALSE(r.contains({40, 30}));
  STATIC_REQUIRE_FALSE(r.contains({20, 60}));
  STATIC_REQUIRE(rect{{0, 0}, {0, 5}}.empty());
  STATIC_REQUIRE(rect::from_edges(5, 5, 1, 1).empty());
}

TEST_CASE("inset and outset apply per-edge insets", "[geometry]") {
  constexpr rect r{{0, 0}, {100, 50}};
  STATIC_REQUIRE(inset(r, insets::symmetric(10, 5)) == rect{{10, 5}, {80, 40}});
  STATIC_REQUIRE(outset(r, insets::uniform(2)) == rect{{-2, -2}, {104, 54}});
  STATIC_REQUIRE(inset(r, insets::uniform(60)).empty());
  STATIC_REQUIRE(insets{1, 2, 3, 4}.horizontal() == 6);
  STATIC_REQUIRE(insets{1, 2, 3, 4}.vertical() == 4);
}

TEST_CASE("intersection and union", "[geometry]") {
  constexpr rect a{{0, 0}, {10, 10}};
  constexpr rect b{{5, 5}, {10, 10}};
  constexpr rect far{{20, 20}, {1, 1}};
  STATIC_REQUIRE(intersection(a, b) == rect{{5, 5}, {5, 5}});
  STATIC_REQUIRE(intersection(a, far).empty());
  STATIC_REQUIRE_FALSE(intersects(a, rect{{10, 0}, {5, 5}}));
  STATIC_REQUIRE(bounding_union(a, b) == rect{{0, 0}, {15, 15}});
  STATIC_REQUIRE(bounding_union(rect{}, b) == b);
  STATIC_REQUIRE(translated(a, {3, 4}) == rect{{3, 4}, {10, 10}});
}

TEST_CASE("affine transforms compose inner first", "[geometry]") {
  constexpr auto t = affine_transform::translation(10, 0) * affine_transform::scaling(2, 3);
  STATIC_REQUIRE(apply(t, {1, 1}) == point{12, 3});
  STATIC_REQUIRE(affine_transform::identity().is_identity());
  STATIC_REQUIRE(t.preserves_axis_alignment());

  auto const quarter = affine_transform::rotation(std::numbers::pi_v<float> / 2);
  auto const p = apply(quarter, {1, 0});
  REQUIRE_THAT(p.x, WithinAbs(0, 1e-6));
  REQUIRE_THAT(p.y, WithinAbs(1, 1e-6));
  REQUIRE(quarter.preserves_axis_alignment());
  REQUIRE_FALSE(affine_transform::rotation(0.3f).preserves_axis_alignment());
}

TEST_CASE("transform_bounds covers every rotated corner", "[geometry]") {
  auto const t = affine_transform::rotation(std::numbers::pi_v<float> / 4);
  auto const bounds = transform_bounds(t, {{0, 0}, {2, 2}});
  float const diagonal = 2 * std::numbers::sqrt2_v<float>;
  REQUIRE_THAT(bounds.width(), WithinAbs(diagonal, 1e-5));
  REQUIRE_THAT(bounds.height(), WithinAbs(diagonal, 1e-5));
  REQUIRE_THAT(bounds.left(), WithinAbs(-std::numbers::sqrt2_v<float>, 1e-5));
}

TEST_CASE("invert undoes a transform and rejects singular ones", "[geometry]") {
  auto const t = affine_transform::translation(5, -3) * affine_transform::rotation(0.7f) *
                 affine_transform::scaling(2, 4);
  auto const inverse = invert(t);
  REQUIRE(inverse.has_value());
  auto const round_trip = apply(*inverse, apply(t, {3, 7}));
  REQUIRE_THAT(round_trip.x, WithinAbs(3, 1e-4));
  REQUIRE_THAT(round_trip.y, WithinAbs(7, 1e-4));
  REQUIRE_FALSE(invert(affine_transform::scaling(0, 1)).has_value());
}
