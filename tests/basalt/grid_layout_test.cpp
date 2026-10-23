#include <catch2/catch_test_macros.hpp>

#include "basalt/widgets/grid_layout.h"

#include <array>
#include <vector>

using namespace basalt;

namespace {

// 100x50 cells with a gap of 10, so a pitch of 110 across and 60 down.
constexpr size cell{100, 50};
constexpr float gap = 10;

auto make(float width, std::vector<grid_section> const &sections, float header_height = 30)
    -> grid_layout {
  return {width, cell, gap, header_height, sections};
}

} // namespace

TEST_CASE("columns are the most cells that fit in the width", "[grid_layout]") {
  std::vector<grid_section> const one{{100, false}};
  REQUIRE(make(100, one).columns() == 1);
  REQUIRE(make(209, one).columns() == 1);
  REQUIRE(make(210, one).columns() == 2);
  REQUIRE(make(319, one).columns() == 2);
  REQUIRE(make(320, one).columns() == 3);
}

TEST_CASE("a grid narrower than a cell still has one column", "[grid_layout]") {
  std::vector<grid_section> const one{{4, false}};
  auto const g = make(20, one);
  REQUIRE(g.columns() == 1);
  REQUIRE(g.item_rect(3).origin.y == 3 * 60);
}

TEST_CASE("items fill rows from the left", "[grid_layout]") {
  std::vector<grid_section> const one{{7, false}};
  auto const g = make(330, one);
  REQUIRE(g.item_count() == 7);
  REQUIRE(g.item_rect(0) == rect{{0, 0}, cell});
  REQUIRE(g.item_rect(2) == rect{{220, 0}, cell});
  REQUIRE(g.item_rect(3) == rect{{0, 60}, cell});
  REQUIRE(g.item_rect(6) == rect{{0, 120}, cell});
  // Three rows of 50 with two gaps of 10.
  REQUIRE(g.total_height() == 170);
}

TEST_CASE("an empty grid has no height and no visible items", "[grid_layout]") {
  auto const none = make(300, {});
  REQUIRE(none.item_count() == 0);
  REQUIRE(none.total_height() == 0);
  REQUIRE(none.visible_items(0, 100).empty());
  REQUIRE(none.visible_headers(0, 100).empty());
  auto const empty_section = make(300, {{0, true}});
  REQUIRE(empty_section.total_height() == 30);
  REQUIRE(empty_section.visible_items(0, 100).empty());
  REQUIRE(empty_section.visible_headers(0, 100) == index_range{0, 1});
}

TEST_CASE("sections start on their own rows below their headers", "[grid_layout]") {
  // Two columns. Section 0: header and 3 items. Section 1: no header, 2 items.
  std::vector<grid_section> const sections{{3, true}, {2, false}};
  auto const g = make(210, sections);
  REQUIRE(g.columns() == 2);
  REQUIRE(g.header_rect(0) == rect{{0, 0}, {210, 30}});
  REQUIRE(g.header_rect(1).empty());
  // Header 30, gap 10, then rows at 40 and 100.
  REQUIRE(g.item_rect(0).origin == point{0, 40});
  REQUIRE(g.item_rect(2).origin == point{0, 100});
  // The first section ends at 150. A gap of 10 separates the next one.
  REQUIRE(g.item_rect(3).origin == point{0, 160});
  REQUIRE(g.item_rect(4).origin == point{110, 160});
  REQUIRE(g.total_height() == 210);
  REQUIRE(g.section_of(2) == 0);
  REQUIRE(g.section_of(3) == 1);
  REQUIRE(g.section_items(1) == index_range{3, 5});
}

TEST_CASE("empty sections do not break item lookup", "[grid_layout]") {
  std::vector<grid_section> const sections{{0, false}, {2, false}, {0, true}, {1, false}};
  auto const g = make(330, sections);
  REQUIRE(g.section_of(0) == 1);
  REQUIRE(g.section_of(1) == 1);
  REQUIRE(g.section_of(2) == 3);
}

TEST_CASE("visible items are the rows that overlap the span", "[grid_layout]") {
  std::vector<grid_section> const one{{20, false}};
  auto const g = make(330, one);
  // Rows start at 0, 60, 120, ... and are 50 tall.
  REQUIRE(g.visible_items(0, 50) == index_range{0, 3});
  REQUIRE(g.visible_items(0, 51) == index_range{0, 3});
  REQUIRE(g.visible_items(0, 61) == index_range{0, 6});
  REQUIRE(g.visible_items(55, 70) == index_range{3, 6});
  REQUIRE(g.visible_items(100, 130) == index_range{3, 9});
  REQUIRE(g.visible_items(115, 130) == index_range{6, 9});
  REQUIRE(g.visible_items(0, 1000) == index_range{0, 20});
  REQUIRE(g.visible_items(5000, 6000).empty());
  REQUIRE(g.visible_items(100, 100).empty());
}

TEST_CASE("a margin adds whole rows on each side and stops at the ends", "[grid_layout]") {
  std::vector<grid_section> const one{{20, false}};
  auto const g = make(330, one);
  REQUIRE(g.visible_items(240, 290, 1) == index_range{9, 18});
  REQUIRE(g.visible_items(0, 50, 2) == index_range{0, 9});
  REQUIRE(g.visible_items(360, 410, 3) == index_range{9, 20});
}

TEST_CASE("the visible range spans section boundaries and skips headers", "[grid_layout]") {
  std::vector<grid_section> const sections{{3, true}, {3, true}};
  auto const g = make(330, sections);
  // Section 0: header 0..30, items at 40. Section 1: starts at 100, header, items at 140.
  REQUIRE(g.item_rect(3).origin.y == 140);
  REQUIRE(g.visible_items(0, 35).empty());
  REQUIRE(g.visible_items(0, 45) == index_range{0, 3});
  REQUIRE(g.visible_items(60, 150) == index_range{0, 6});
  REQUIRE(g.visible_items(95, 150) == index_range{3, 6});
  REQUIRE(g.visible_headers(0, 35) == index_range{0, 1});
  REQUIRE(g.visible_headers(35, 95).empty());
  REQUIRE(g.visible_headers(0, 120) == index_range{0, 2});
  REQUIRE(g.visible_headers(120, 135) == index_range{1, 2});
}

TEST_CASE("moving up and down keeps the column across rows and sections", "[grid_layout]") {
  // Three columns: section 0 has 5 items (rows of 3 and 2), section 1 has 4 (3 and 1).
  std::vector<grid_section> const sections{{5, true}, {4, false}};
  auto const g = make(330, sections);
  REQUIRE(g.item_above(4) == 1);
  REQUIRE(g.item_above(1) == std::nullopt);
  REQUIRE(g.item_below(1) == 4);
  // Item 2 is in the last column; the row below has only two items.
  REQUIRE(g.item_below(2) == 4);
  // Below the last row of section 0 is the first row of section 1.
  REQUIRE(g.item_below(3) == 5);
  REQUIRE(g.item_below(4) == 6);
  REQUIRE(g.item_above(5) == 3);
  REQUIRE(g.item_above(6) == 4);
  REQUIRE(g.item_above(8) == 5);
  REQUIRE(g.item_below(7) == 8);
  REQUIRE(g.item_below(8) == std::nullopt);
}

TEST_CASE("queries on fifty thousand items return a small range", "[grid_layout]") {
  std::vector<grid_section> const sections{{50'000, false}};
  auto const g = make(1000, sections);
  REQUIRE(g.columns() == 9);
  REQUIRE(g.item_count() == 50'000);
  // Row 5000 starts at y = 300000 and holds item 45000.
  auto const range = g.visible_items(300'000, 300'600, 2);
  REQUIRE(range.size() < 150);
  REQUIRE(range.contains(45'000));
  REQUIRE(g.visible_items(0, 10'000'000).size() == 50'000);
}
