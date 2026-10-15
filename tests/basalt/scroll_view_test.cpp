#include <catch2/catch_test_macros.hpp>

#include "basalt/widget_tree.h"
#include "basalt/widgets/scroll_view.h"

#include <memory>

#include "leaf.h"

using namespace basalt;
using basalt::testing::leaf;

namespace {

struct fixture {
  explicit fixture(scroll_axes axes = scroll_axes::vertical, scale_factors scale = {})
      : tree({}, scale) {
    tree.set_viewport({100, 50});
    view = &tree.emplace_root<scroll_view>(axes);
    content = &view->emplace_content<leaf>(size{40, 200});
    tree.update_layout();
  }

  widget_tree tree;
  scroll_view *view = nullptr;
  leaf *content = nullptr;
};

} // namespace

TEST_CASE("vertical scrolling sizes content to the view's width", "[layout][scroll_view]") {
  fixture f;
  REQUIRE(f.view->content() == f.content);
  REQUIRE(f.view->content_size() == size{100, 200});
  REQUIRE(f.content->bounds() == rect{{0, 0}, {100, 200}});
  REQUIRE(f.view->max_scroll_offset() == point{0, 150});

  REQUIRE(f.view->set_scroll_offset({30, 60}));
  REQUIRE(f.view->scroll_offset() == point{0, 60});
  REQUIRE(f.content->bounds().origin == point{0, -60});
  REQUIRE(f.view->visible_content_rect() == rect{{0, 60}, {100, 50}});

  REQUIRE(f.view->scroll_by({0, 1000}));
  REQUIRE(f.view->scroll_offset() == point{0, 150});
  REQUIRE_FALSE(f.view->scroll_by({0, 10}));
  REQUIRE(f.view->set_scroll_offset({0, -5}));
  REQUIRE(f.view->scroll_offset() == point{});
}

TEST_CASE("small content fills the view", "[layout][scroll_view]") {
  fixture f(scroll_axes::both);
  f.content->set_wanted({10, 10});
  f.tree.update_layout();
  REQUIRE(f.view->content_size() == size{100, 50});
  REQUIRE(f.view->max_scroll_offset() == point{});
}

TEST_CASE("scrolling into view moves as little as possible", "[layout][scroll_view]") {
  fixture f(scroll_axes::both);
  f.content->set_wanted({300, 200});
  f.tree.update_layout();

  REQUIRE_FALSE(f.view->scroll_into_view({{10, 10}, {10, 10}}));
  REQUIRE(f.view->scroll_into_view({{150, 100}, {20, 20}}));
  REQUIRE(f.view->scroll_offset() == point{70, 70});
  REQUIRE(f.view->scroll_into_view({{50, 60}, {10, 5}}));
  REQUIRE(f.view->scroll_offset() == point{50, 60});

  SECTION("an area larger than the view shows its start") {
    REQUIRE(f.view->scroll_into_view({{120, 80}, {150, 10}}));
    REQUIRE(f.view->scroll_offset() == point{120, 60});
  }
}

TEST_CASE("offsets requested before layout are clamped by the next layout",
          "[layout][scroll_view]") {
  fixture f;
  f.content->set_wanted({40, 400});
  REQUIRE(f.view->needs_layout());
  REQUIRE(f.view->scroll_into_view({{0, 380}, {10, 20}}));
  f.tree.update_layout();
  REQUIRE(f.view->scroll_offset() == point{0, 350});
  REQUIRE(f.content->bounds().origin == point{0, -350});

  SECTION("shrinking content pulls the offset back") {
    f.content->set_wanted({40, 100});
    f.tree.update_layout();
    REQUIRE(f.view->scroll_offset() == point{0, 50});
  }

  SECTION("disabling an axis resets its offset") {
    f.view->set_axes(scroll_axes::horizontal);
    f.tree.update_layout();
    REQUIRE(f.view->scroll_offset() == point{});
    REQUIRE(f.view->content_size().height == 50);
  }
}

TEST_CASE("scrolling damages only the view", "[layout][scroll_view][paint]") {
  fixture f;
  (void)f.tree.take_damage();
  f.tree.set_viewport({200, 200});
  f.tree.update_layout();
  f.view->set_bounds({{10, 10}, {100, 50}});
  f.tree.update_layout();
  (void)f.tree.take_damage();

  REQUIRE(f.view->scroll_by({0, 20}));
  auto const damage = f.tree.take_damage();
  REQUIRE(damage.size() == 1);
  REQUIRE(damage[0] == rect{{10, 10}, {100, 50}});
}

TEST_CASE("scroll offsets snap to device pixels", "[layout][scroll_view][scaling]") {
  fixture f(scroll_axes::vertical, {.display = 2});
  REQUIRE(f.view->set_scroll_offset({0, 10.3f}));
  REQUIRE(f.view->scroll_offset() == point{0, 10.5f});
}

TEST_CASE("replacing or removing the content resets scrolling", "[layout][scroll_view]") {
  fixture f;
  REQUIRE(f.view->set_scroll_offset({0, 40}));

  auto &next = f.view->set_content(std::make_unique<leaf>(size{10, 500}));
  REQUIRE(f.view->child_count() == 1);
  REQUIRE(f.view->content() == &next);
  REQUIRE(f.view->scroll_offset() == point{});
  f.tree.update_layout();
  REQUIRE(f.view->content_size() == size{100, 500});

  auto owned = f.view->remove_child(next);
  REQUIRE(f.view->content() == nullptr);
  f.tree.update_layout();
  REQUIRE(f.view->content_size() == size{});
  REQUIRE(f.view->measure({}) == size{});
}

namespace {

// Notes each time the view says that the visible part moved.
struct watcher : leaf {
  using leaf::leaf;
  int moves = 0;

protected:
  auto on_viewport_moved() -> void override { ++moves; }
};

} // namespace

TEST_CASE("scrolling tells the content that the visible part moved", "[layout][scroll_view]") {
  widget_tree tree;
  tree.set_viewport({100, 50});
  auto &view = tree.emplace_root<scroll_view>();
  auto &content = view.emplace_content<watcher>(size{40, 200});
  tree.update_layout();
  REQUIRE(content.moves == 0);

  view.set_scroll_offset({0, 20});
  CHECK(content.moves == 1);
  // Nothing moved, so nothing is reported.
  view.set_scroll_offset({0, 20});
  CHECK(content.moves == 1);
  view.scroll_by({0, 10});
  CHECK(content.moves == 2);
}
