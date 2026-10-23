#include <catch2/catch_test_macros.hpp>

#include "basalt/widgets/collection_view.h"

#include <cstddef>
#include <memory>
#include <vector>

#include "ui_fixture.h"

using namespace basalt;
using namespace basalt::testing;
namespace a11y = basalt::accessibility;

namespace {

// A cell that remembers which item it shows.
struct tag : widget {
  std::size_t index = 0;
  int updates = 0;
};

// The default viewport is 400x300. With 100x100 cells and no gap that is four
// columns and three rows in view; a margin of one row adds a fourth.
struct fixture {
  explicit fixture(std::size_t count = 10'000, std::size_t margin = 1) {
    view = &f.tree.emplace_root<collection_view>("Photos");
    view->set_cell_size(fixed(100), fixed(100));
    view->set_gap(fixed(0));
    view->set_margin_rows(margin);
    view->set_cell_factory([this](std::size_t index, std::unique_ptr<widget> recycled) {
      std::unique_ptr<tag> cell;
      if (recycled != nullptr) {
        ++recycled_count;
        cell.reset(static_cast<tag *>(recycled.release()));
      } else {
        ++created;
        cell = std::make_unique<tag>();
      }
      cell->index = index;
      ++cell->updates;
      return cell;
    });
    view->set_on_visible_change([this](index_range r) { ranges.push_back(r); });
    view->set_on_current_change([this](std::size_t i) { moves.push_back(i); });
    view->set_on_activate([this](std::size_t i) { activations.push_back(i); });
    view->set_item_count(count);
    f.layout();
  }

  [[nodiscard]] auto realized_widgets() const -> std::size_t {
    return view->child(0).child(0).child_count();
  }

  ui f;
  collection_view *view = nullptr;
  int created = 0;
  int recycled_count = 0;
  std::vector<index_range> ranges;
  std::vector<std::size_t> moves;
  std::vector<std::size_t> activations;
};

} // namespace

TEST_CASE("a grid makes widgets only for the rows in view and the margin", "[collection_view]") {
  fixture g;
  REQUIRE(g.view->layout().columns() == 4);
  REQUIRE(g.view->visible_items() == index_range{0, 12});
  REQUIRE(g.view->realized_items() == index_range{0, 16});
  REQUIRE(g.realized_widgets() == 16);
  REQUIRE(g.created == 16);
  REQUIRE(g.view->cell(15) != nullptr);
  REQUIRE(g.view->cell(16) == nullptr);
}

TEST_CASE("cells receive their index and sit at their grid positions", "[collection_view]") {
  fixture g;
  auto const *const cell = static_cast<tag const *>(g.view->cell(5));
  REQUIRE(cell != nullptr);
  REQUIRE(cell->index == 5);
  REQUIRE(cell->bounds() == rect{{100, 100}, {100, 100}});
}

TEST_CASE("scrolling reuses cells instead of making new ones", "[collection_view]") {
  fixture g;
  int const created_at_start = g.created;
  g.view->set_scroll_offset(50'000);
  g.f.layout();
  // Row 500 holds item 2000.
  REQUIRE(g.view->visible_items().contains(2000));
  // Row 500 is in view with a margin row on each side.
  REQUIRE(g.realized_widgets() == 20);
  REQUIRE(g.created == created_at_start + 4);
  REQUIRE(g.recycled_count == 16);
  REQUIRE(g.view->cell(0) == nullptr);
  auto const *const cell = static_cast<tag const *>(g.view->cell(2000));
  REQUIRE(cell != nullptr);
  REQUIRE(cell->index == 2000);
}

TEST_CASE("scrolling slowly creates a cell only when the pool is empty", "[collection_view]") {
  fixture g;
  for (int step = 0; step < 50; ++step) {
    g.view->set_scroll_offset(g.view->scroll_offset() + 40);
    g.f.layout();
  }
  REQUIRE(g.realized_widgets() <= 24);
  REQUIRE(g.created <= 24);
}

TEST_CASE("the visible range is reported when it changes and not otherwise", "[collection_view]") {
  fixture g;
  REQUIRE(g.ranges == std::vector<index_range>{{0, 12}});
  g.view->set_scroll_offset(0);
  g.f.layout();
  g.view->reload();
  g.f.layout();
  REQUIRE(g.ranges.size() == 1);
  g.view->set_scroll_offset(100);
  g.f.layout();
  REQUIRE(g.ranges.size() == 2);
  REQUIRE(g.ranges.back() == index_range{4, 16});
}

TEST_CASE("a grid scrolls to an item before its first layout", "[collection_view]") {
  ui f;
  auto &view = f.tree.emplace_root<collection_view>();
  view.set_cell_size(fixed(100), fixed(100));
  view.set_gap(fixed(0));
  view.set_item_count(1000);
  view.scroll_to_item(400);
  f.layout();
  // Item 400 is in row 100, at 10000.
  REQUIRE(view.scroll_offset() >= 10'000 + 100 - 300);
  REQUIRE(view.visible_items().contains(400));
}

TEST_CASE("fewer items remove the cells beyond the end", "[collection_view]") {
  fixture g;
  g.view->set_item_count(6);
  g.f.layout();
  REQUIRE(g.realized_widgets() == 6);
  REQUIRE(g.view->cell(6) == nullptr);
  g.view->set_item_count(0);
  g.f.layout();
  REQUIRE(g.realized_widgets() == 0);
  REQUIRE(g.view->visible_items().empty());
}

TEST_CASE("reload runs the factory again with the cell it already made", "[collection_view]") {
  fixture g;
  auto *const before = static_cast<tag *>(g.view->cell(3));
  REQUIRE(before->updates == 1);
  int const created = g.created;
  g.view->reload();
  g.f.layout();
  REQUIRE(g.view->cell(3) == before);
  REQUIRE(before->updates == 2);
  REQUIRE(g.created == created);
}

TEST_CASE("a narrower grid has fewer columns and more rows", "[collection_view]") {
  fixture g;
  g.f.tree.set_viewport({200, 300});
  g.f.layout();
  REQUIRE(g.view->layout().columns() == 2);
  REQUIRE(g.view->visible_items() == index_range{0, 6});
  auto const *const cell = static_cast<tag const *>(g.view->cell(3));
  REQUIRE(cell->bounds() == rect{{100, 100}, {100, 100}});
}

TEST_CASE("arrow keys move the current item across rows and scroll to it", "[collection_view]") {
  fixture g;
  g.f.tap(key::tab);
  REQUIRE(g.view->has_focus());
  g.f.tap(key::down);
  REQUIRE(g.view->current() == 0);
  g.f.tap(key::right);
  g.f.tap(key::down);
  REQUIRE(g.view->current() == 5);
  g.f.tap(key::left);
  REQUIRE(g.view->current() == 4);
  g.f.tap(key::up);
  REQUIRE(g.view->current() == 0);
  REQUIRE(g.moves == std::vector<std::size_t>{0, 1, 5, 4, 0});
  for (int i = 0; i < 6; ++i) {
    g.f.tap(key::down);
  }
  REQUIRE(g.view->current() == 24);
  REQUIRE(g.view->visible_items().contains(24));
}

TEST_CASE("Home, End and the page keys jump through the grid", "[collection_view]") {
  fixture g(100);
  g.f.tap(key::tab);
  g.f.tap(key::end);
  REQUIRE(g.view->current() == 99);
  REQUIRE(g.view->visible_items().contains(99));
  g.f.tap(key::page_up);
  // Three rows of four.
  REQUIRE(g.view->current() == 87);
  g.f.tap(key::home);
  REQUIRE(g.view->current() == 0);
  REQUIRE(g.view->scroll_offset() == 0);
  g.f.tap(key::page_down);
  REQUIRE(g.view->current() == 12);
}

TEST_CASE("Enter and Space activate the current item", "[collection_view]") {
  fixture g;
  g.f.tap(key::tab);
  g.f.tap(key::enter);
  REQUIRE(g.activations.empty());
  g.f.tap(key::right);
  g.f.tap(key::right);
  g.f.tap(key::enter);
  g.f.tap(key::space);
  // The first arrow key picks item 0 and the second moves to item 1.
  REQUIRE(g.activations == std::vector<std::size_t>{1, 1});
}

TEST_CASE("a click on a cell makes it current and activates it", "[collection_view]") {
  fixture g;
  g.f.click(g.view->cell(6)->bounds_in_root().center());
  REQUIRE(g.view->current() == 6);
  REQUIRE(g.activations == std::vector<std::size_t>{6});
  REQUIRE(g.view->has_focus());
}

TEST_CASE("a press that ends on another cell does not activate", "[collection_view]") {
  fixture g;
  g.f.send(press(g.view->cell(1)->bounds_in_root().center()));
  g.f.send(release(g.view->cell(2)->bounds_in_root().center()));
  REQUIRE(g.activations.empty());
}

TEST_CASE("section headers appear while they are in view", "[collection_view]") {
  fixture g(0);
  std::vector<std::size_t> headers_made;
  g.view->set_header_height(fixed(40));
  g.view->set_header_factory([&](std::size_t section) {
    headers_made.push_back(section);
    return std::make_unique<widget>();
  });
  g.view->set_sections({{8, true}, {400, true}, {8, true}});
  g.f.layout();
  REQUIRE(headers_made == std::vector<std::size_t>{0, 1});
  // Header 0 is 40 tall, and so are the rows below it; section 1 starts at 40 + 200.
  REQUIRE(g.view->layout().header_rect(1).origin.y == 240);
  g.view->set_scroll_offset(5'000);
  g.f.layout();
  REQUIRE(g.view->visible_items().first >= g.view->layout().section_items(1).first);
  REQUIRE(headers_made.size() == 2);
  g.view->set_scroll_offset(g.view->layout().total_height());
  g.f.layout();
  REQUIRE(headers_made.back() == 2);
}

TEST_CASE("the focus ring surrounds the current cell", "[collection_view]") {
  fixture g;
  g.f.tap(key::tab);
  g.f.tap(key::down);
  g.f.tap(key::right);
  auto const ring = g.view->focus_ring();
  REQUIRE(ring.has_value());
  REQUIRE(ring->area == rect{{100, 0}, {100, 100}});
  g.view->set_scroll_offset(50);
  g.f.layout();
  REQUIRE(g.view->focus_ring()->area == rect{{100, -50}, {100, 100}});
}

TEST_CASE("a grid is a named list for assistive technology", "[collection_view][accessibility]") {
  fixture g;
  auto const info = g.view->accessibility_info();
  REQUIRE(info.kind == a11y::role::list);
  REQUIRE(info.name == "Photos");
}

TEST_CASE("setting the current item scrolls to it and stays quiet", "[collection_view]") {
  fixture g;
  g.view->set_current(800);
  g.f.layout();
  REQUIRE(g.view->current() == 800);
  REQUIRE(g.view->visible_items().contains(800));
  REQUIRE(g.moves.empty());
  g.view->set_current(99'999);
  REQUIRE(g.view->current() == 9'999);
}
