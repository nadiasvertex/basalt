#include <catch2/catch_test_macros.hpp>

#include "basalt/layout.h"
#include "basalt/widget_tree.h"

#include <vector>

#include "leaf.h"

using namespace basalt;

TEST_CASE("distribute shares free space by grow factor", "[layout]") {
  std::vector<linear_item> const items{
      {.basis = 10, .grow = 1},
      {.basis = 10, .grow = 3},
      {.basis = 10},
  };
  REQUIRE(distribute(items, 70) == std::vector<float>{20, 40, 10});

  SECTION("without growers the free space is left over") {
    std::vector<linear_item> const fixed_items{{.basis = 10}, {.basis = 20}};
    REQUIRE(distribute(fixed_items, 100) == std::vector<float>{10, 20});
  }
}

TEST_CASE("distribute freezes items at their limits and redistributes", "[layout]") {
  SECTION("a capped grower hands the rest to the others") {
    std::vector<linear_item> const items{
        {.basis = 0, .max = 10, .grow = 1},
        {.basis = 0, .grow = 1},
    };
    REQUIRE(distribute(items, 100) == std::vector<float>{10, 90});
  }

  SECTION("shrinking is weighted by basis and stops at the minimum") {
    std::vector<linear_item> const items{
        {.basis = 100, .shrink = 1},
        {.basis = 300, .shrink = 1},
    };
    REQUIRE(distribute(items, 200) == std::vector<float>{50, 150});

    std::vector<linear_item> const limited{
        {.basis = 100, .min = 90, .shrink = 1},
        {.basis = 300, .shrink = 1},
    };
    REQUIRE(distribute(limited, 200) == std::vector<float>{90, 110});
  }

  SECTION("an impossible fit overflows") {
    std::vector<linear_item> const items{{.basis = 50, .min = 50, .shrink = 1}};
    REQUIRE(distribute(items, 10) == std::vector<float>{50});
  }

  SECTION("the basis is clamped before distributing") {
    std::vector<linear_item> const items{{.basis = 5, .min = 20}, {.basis = 50, .max = 30}};
    REQUIRE(distribute(items, 100) == std::vector<float>{20, 30});
  }
}

TEST_CASE("constraints clamp sizes and deflate by insets", "[layout]") {
  constraints const c{{10, 10}, {100, unbounded}};
  REQUIRE(c.constrain({5, 500}) == size{10, 500});
  REQUIRE(c.constrain({200, 50}) == size{100, 50});
  REQUIRE(c.bounded_width());
  REQUIRE_FALSE(c.bounded_height());
  REQUIRE(constraints::tight({4, 5}).constrain({}) == size{4, 5});

  constraints const inner = c.deflate(insets::uniform(8));
  REQUIRE(inner.min == size{0, 0});
  REQUIRE(inner.max.width == 84);
  REQUIRE_FALSE(inner.bounded_height());
  REQUIRE(inflate({1, 2}, insets{1, 2, 3, 4}) == size{7, 6});
}

TEST_CASE("axis helpers swap coordinates", "[layout]") {
  REQUIRE(make_size(axis::horizontal, 1, 2) == size{1, 2});
  REQUIRE(make_size(axis::vertical, 1, 2) == size{2, 1});
  REQUIRE(make_point(axis::vertical, 1, 2) == point{2, 1});
  REQUIRE(main_extent({3, 4}, axis::vertical) == 4);
  REQUIRE(cross_extent({3, 4}, axis::vertical) == 3);
  REQUIRE(cross_axis(axis::horizontal) == axis::vertical);
  REQUIRE(align_offset(alignment::center, 10) == 5);
  REQUIRE(align_offset(alignment::end, 10) == 10);
  REQUIRE(align_offset(alignment::stretch, 10) == 0);
}

TEST_CASE("length insets and lengths resolve under scale factors", "[layout][units]") {
  scale_factors const doubled_text{.text = 2};
  auto const edges = length_insets::symmetric(fixed(4), relative(8));
  REQUIRE(to_layout(edges, doubled_text) == insets{16, 4, 16, 4});
  REQUIRE(to_layout(edges, {.zoom = 2}) == insets{16, 8, 16, 8});

  REQUIRE(from_layout(40, unit::relative, {.zoom = 2, .text = 2}) == relative(10));
  REQUIRE(from_layout(40, unit::fixed, {.zoom = 2, .text = 2}) == fixed(20));
}

TEST_CASE("widgets measure within constraints and resolve with their tree's scale", "[layout]") {
  testing::leaf l({50, 500});
  REQUIRE(l.measure({{}, {100, 100}}) == size{50, 100});
  REQUIRE(l.measure({{60, 0}, {100, 1000}}) == size{60, 500});
  REQUIRE(l.scale() == scale_factors{});
  REQUIRE(l.resolve(relative(10)) == 10);

  widget_tree tree({}, {.display = 2, .text = 2});
  auto &root = tree.emplace_root<testing::leaf>();
  REQUIRE(root.resolve(relative(10)) == 20);
  REQUIRE(root.resolve(fixed(10)) == 10);
  REQUIRE(root.resolve(length_insets::uniform(relative(1))) == insets::uniform(2));
}
