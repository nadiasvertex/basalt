#include <catch2/catch_test_macros.hpp>

#include "basalt/widget_tree.h"
#include "basalt/widgets/split_view.h"

#include <memory>

#include "leaf.h"

using namespace basalt;
using basalt::testing::leaf;

namespace {

struct fixture {
  explicit fixture(axis direction = axis::horizontal, scale_factors scale = {}) : tree({}, scale) {
    tree.set_viewport({201, 100});
    split = &tree.emplace_root<split_view>(direction);
    split->set_divider_thickness(fixed(1));
    first = &split->set_first(std::make_unique<leaf>(size{30, 10}));
    second = &split->set_second(std::make_unique<leaf>(size{50, 20}));
    tree.update_layout();
  }

  widget_tree tree;
  split_view *split = nullptr;
  leaf *first = nullptr;
  leaf *second = nullptr;
};

} // namespace

TEST_CASE("a proportional split keeps its ratio when resized", "[layout][split_view]") {
  fixture f;
  REQUIRE(f.split->anchor() == split_anchor::proportional);
  REQUIRE(f.first->bounds() == rect{{0, 0}, {100, 100}});
  REQUIRE(f.split->divider_rect() == rect{{100, 0}, {1, 100}});
  REQUIRE(f.second->bounds() == rect{{101, 0}, {100, 100}});

  f.split->set_ratio(0.25f);
  f.tree.set_viewport({401, 100});
  f.tree.update_layout();
  REQUIRE(f.first->bounds().width() == 100);
  REQUIRE(f.second->bounds() == rect{{101, 0}, {300, 100}});
}

TEST_CASE("an anchored pane keeps its length when resized", "[layout][split_view]") {
  fixture f;
  f.split->set_first_length(fixed(60));
  f.tree.update_layout();
  REQUIRE(f.first->bounds().width() == 60);
  f.tree.set_viewport({401, 100});
  f.tree.update_layout();
  REQUIRE(f.first->bounds().width() == 60);
  REQUIRE(f.second->bounds().width() == 340);

  f.split->set_second_length(fixed(60));
  f.tree.update_layout();
  REQUIRE(f.second->bounds() == rect{{341, 0}, {60, 100}});
  f.tree.set_viewport({201, 100});
  f.tree.update_layout();
  REQUIRE(f.second->bounds() == rect{{141, 0}, {60, 100}});
}

TEST_CASE("anchored lengths follow zoom and text scale", "[layout][split_view][scaling]") {
  fixture f(axis::horizontal, {.zoom = 2});
  f.tree.set_viewport({401, 100});
  f.split->set_first_length(fixed(60));
  f.tree.update_layout();
  // The divider is fixed too, so it doubles with zoom.
  REQUIRE(f.first->bounds().width() == 120);
  REQUIRE(f.split->divider_rect().width() == 2);

  f.tree.set_scale({.text = 2});
  f.tree.update_layout();
  REQUIRE(f.first->bounds().width() == 60);
  f.split->set_first_length(relative(60));
  f.tree.update_layout();
  REQUIRE(f.first->bounds().width() == 120);
}

TEST_CASE("pane minimums limit the divider", "[layout][split_view]") {
  fixture f;
  f.split->set_minimums(fixed(40), fixed(50));

  REQUIRE(f.split->move_divider_to(10) == 40);
  f.tree.update_layout();
  REQUIRE(f.first->bounds().width() == 40);
  REQUIRE(f.split->ratio() == 0.2f);

  REQUIRE(f.split->move_divider_to(190) == 150);
  f.tree.update_layout();
  REQUIRE(f.second->bounds().width() == 50);

  SECTION("the first minimum wins when both cannot fit") {
    f.tree.set_viewport({61, 100});
    f.tree.update_layout();
    REQUIRE(f.first->bounds().width() == 40);
    REQUIRE(f.second->bounds().width() == 20);
  }

  SECTION("moving by a delta starts from the current position") {
    REQUIRE(f.split->move_divider_by(-30) == 120);
  }
}

TEST_CASE("moving the divider keeps the anchor kind and unit", "[layout][split_view]") {
  fixture f(axis::horizontal, {.text = 2});
  f.split->set_second_length(relative(20));
  f.tree.update_layout();
  REQUIRE(f.second->bounds().width() == 40);

  REQUIRE(f.split->move_divider_to(100) == 100);
  REQUIRE(f.split->anchor() == split_anchor::second);
  REQUIRE(f.split->anchored_length() == relative(50));
}

TEST_CASE("a vertical split stacks its panes", "[layout][split_view]") {
  fixture f(axis::vertical);
  f.tree.set_viewport({100, 201});
  f.tree.update_layout();
  REQUIRE(f.first->bounds() == rect{{0, 0}, {100, 100}});
  REQUIRE(f.split->divider_rect() == rect{{0, 100}, {100, 1}});
  REQUIRE(f.second->bounds() == rect{{0, 101}, {100, 100}});
  REQUIRE(f.split->measure({}) == size{50, 31});
}

TEST_CASE("a single pane fills the split", "[layout][split_view]") {
  fixture f;
  REQUIRE(f.split->measure({}) == size{81, 20});

  f.first->set_visible(false);
  f.tree.update_layout();
  REQUIRE(f.second->bounds() == rect{{0, 0}, {201, 100}});
  REQUIRE(f.split->divider_rect().empty());
  REQUIRE(f.split->measure({}) == size{50, 20});

  auto owned = f.split->remove_child(*f.second);
  REQUIRE(f.split->second() == nullptr);
  f.first->set_visible(true);
  f.tree.update_layout();
  REQUIRE(f.first->bounds() == rect{{0, 0}, {201, 100}});

  auto &replacement = f.split->set_first(std::make_unique<leaf>());
  REQUIRE(f.split->first() == &replacement);
  REQUIRE(f.split->child_count() == 1);
}
