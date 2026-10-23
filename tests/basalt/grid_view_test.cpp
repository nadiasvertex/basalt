#include <catch2/catch_test_macros.hpp>

#include "basalt/render_context.h"
#include "basalt/widget_painter.h"
#include "basalt/widget_tree.h"
#include "basalt/widgets/grid_view.h"
#include "basalt/widgets/scroll_view.h"
#include "basalt/widgets/stack.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

using namespace basalt;

namespace {

// A 1000 × 220 view onto a grid of 100 px cells with 10 px gaps: 9 columns, and two rows show.
struct fixture {
  explicit fixture(std::size_t count = 100, basalt::size viewport = {1000, 220}) {
    tree.set_viewport(viewport);
    view = &tree.emplace_root<scroll_view>();
    grid = &view->emplace_content<grid_view>();
    grid->set_cell_size(100);
    grid->set_gap(10);
    grid->set_item_count(count);
    grid->set_cell_painter([this](render_context &, std::size_t index, rect cell) {
      painted.push_back(index);
      cells.push_back(cell);
    });
    tree.update_layout();
  }

  auto paint() -> std::vector<std::size_t> {
    painted.clear();
    cells.clear();
    paint_commands commands;
    paint_tree(tree, commands);
    return painted;
  }

  widget_tree tree;
  scroll_view *view = nullptr;
  grid_view *grid = nullptr;
  std::vector<std::size_t> painted;
  std::vector<rect> cells;
};

auto range(std::size_t first, std::size_t last) -> std::vector<std::size_t> {
  std::vector<std::size_t> out;
  for (auto i = first; i < last; ++i) {
    out.push_back(i);
  }
  return out;
}

} // namespace

TEST_CASE("columns fit cells and gaps", "[layout][grid_view]") {
  grid_view grid;
  grid.set_cell_size(100);
  grid.set_gap(10);
  // 10 + 9 × (100 + 10) = 1000 exactly.
  CHECK(grid.columns_for(1000) == 9);
  CHECK(grid.columns_for(999) == 8);
  // At least one column, however narrow.
  CHECK(grid.columns_for(50) == 1);
  CHECK(grid.columns_for(0) == 1);
}

TEST_CASE("the grid is as tall as its rows and as wide as the view", "[layout][grid_view]") {
  fixture f(18);
  CHECK(f.grid->columns() == 9);
  CHECK(f.grid->rows() == 2);
  // The outer gap, then two rows of cell and gap.
  CHECK(f.grid->bounds().extent == size{1000, 10 + 2 * 110});

  f.grid->set_item_count(19);
  f.tree.update_layout();
  CHECK(f.grid->rows() == 3);
  CHECK(f.view->content_size() == size{1000, 10 + 3 * 110});

  f.grid->set_item_count(0);
  f.tree.update_layout();
  CHECK(f.grid->rows() == 0);
  // An empty grid still fills the view.
  CHECK(f.view->content_size() == size{1000, 220});
}

TEST_CASE("cells go left to right, then top to bottom", "[layout][grid_view]") {
  fixture f;
  CHECK(f.grid->cell_rect(0) == rect{{10, 10}, {100, 100}});
  // Index 10 is row 1, column 1.
  CHECK(f.grid->cell_rect(10) == rect{{120, 120}, {100, 100}});
}

TEST_CASE("hit testing finds cells but not gaps", "[input][grid_view]") {
  fixture f;
  CHECK(f.grid->index_at({15, 15}) == 0);
  CHECK(f.grid->index_at({125, 125}) == 10);
  CHECK(f.grid->index_at({115, 15}) == std::nullopt);
  CHECK(f.grid->index_at({5, 15}) == std::nullopt);
  CHECK(f.grid->index_at({995, 15}) == std::nullopt);

  fixture few(5);
  CHECK(few.grid->index_at({15, 125}) == std::nullopt);
}

TEST_CASE("the nearest index is clamped into the grid", "[input][grid_view]") {
  fixture f(20);
  // A gap belongs to the cell on its left.
  CHECK(f.grid->nearest_index({115, 15}) == 0);
  CHECK(f.grid->nearest_index({-50, -50}) == 0);
  CHECK(f.grid->nearest_index({5000, 5000}) == 19);

  fixture empty(0);
  CHECK(empty.grid->nearest_index({0, 0}) == std::nullopt);
}

TEST_CASE("a rectangle selects the cells it touches", "[input][grid_view]") {
  fixture f;
  CHECK(f.grid->indices_in({{50, 50}, {100, 100}}) == std::vector<std::size_t>{0, 1, 9, 10});
  CHECK(f.grid->indices_in({{111, 111}, {5, 5}}).empty());
}

TEST_CASE("the visible range covers partly visible rows", "[layout][grid_view]") {
  fixture f;
  CHECK(f.grid->visible_range() == index_range{0, 18});

  // A 100 px view at 125 shows 125 to 225. Row 1 spans 120 to 220 and row 2 starts at 230, so
  // only row 1 touches it.
  fixture narrow(100, {1000, 100});
  narrow.view->set_scroll_offset({0, 125});
  CHECK(narrow.grid->visible_range() == index_range{9, 18});

  fixture few(20, {1000, 10'000});
  CHECK(few.grid->visible_range() == index_range{0, 20});
}

TEST_CASE("painting draws only the cells the view shows", "[paint][grid_view]") {
  fixture f(100'000);
  CHECK(f.paint() == range(0, 18));
  CHECK(f.cells.front() == rect{{10, 10}, {100, 100}});

  // Deep in a tall grid: rows 5000 and 5001 start at 10 + 5000 × 110.
  REQUIRE(f.view->set_scroll_offset({0, 10 + 5000 * 110}));
  CHECK(f.paint() == range(45'000, 45'018));
}

TEST_CASE("a view smaller than the window limits the visible part", "[layout][grid_view]") {
  // The scroll view sits 100 px inside a 1000 × 2000 window, so it shows 1800 px of the grid
  // although the window is taller.
  widget_tree tree;
  tree.set_viewport({1000, 2000});
  auto &frame = tree.emplace_root<stack>();
  frame.set_padding(length_insets::symmetric(fixed(0), fixed(100)));
  auto &view = frame.add(std::make_unique<scroll_view>(), stack_item{});
  auto &grid = view.emplace_content<grid_view>();
  grid.set_cell_size(100);
  grid.set_gap(10);
  grid.set_item_count(100'000);
  tree.update_layout();

  CHECK(grid.visible_area() == rect{{0, 0}, {1000, 1800}});
  // ceil((1800 - 10) / 110) = 17 rows of 9.
  CHECK(grid.visible_range() == index_range{0, 17 * 9});
}

TEST_CASE("scrolling reports a changed visible range", "[layout][grid_view]") {
  fixture f(1000);
  std::vector<index_range> reported;
  f.grid->set_on_visible_range_changed([&](index_range r) { reported.push_back(r); });

  // 130 to 350 touches the row bands of rows 1 to 3 (each band is a cell and the gap after it).
  REQUIRE(f.view->set_scroll_offset({0, 130}));
  REQUIRE(reported.size() == 1);
  CHECK(reported.back() == index_range{9, 36});

  // Moving within the same rows reports nothing.
  REQUIRE(f.view->set_scroll_offset({0, 140}));
  CHECK(reported.size() == 1);

  // A layout that changes the columns reports the new range.
  f.tree.set_viewport({500, 220});
  f.tree.update_layout();
  REQUIRE(reported.size() == 2);
  CHECK(f.grid->columns() == 4);
}

TEST_CASE("a changed cell damages only that cell", "[paint][grid_view]") {
  fixture f;
  (void)f.tree.take_damage();
  f.grid->invalidate_cell(10);
  auto const damage = f.tree.take_damage();
  REQUIRE(damage.size() == 1);
  CHECK(damage[0] == rect{{120, 120}, {100, 100}});

  // A cell the view does not show adds no damage.
  f.grid->invalidate_cell(90);
  CHECK(f.tree.take_damage().empty());
}

TEST_CASE("assistive technology sees a named list with the item count",
          "[accessibility][grid_view]") {
  grid_view grid{"Photos"};
  grid.set_item_count(1234);
  auto const details = grid.accessibility_info();
  CHECK(details.kind == accessibility::role::list);
  CHECK(details.name == "Photos");
  CHECK(details.set_size == 1234);
}

TEST_CASE("resizing cells keeps the content in step", "[layout][grid_view]") {
  fixture f;
  f.grid->set_cell_size(200);
  f.tree.update_layout();
  // (1000 - 10) / 210 = 4 columns, 25 rows.
  CHECK(f.grid->columns() == 4);
  CHECK(f.view->content_size().height == 10 + 25 * 210);
}
