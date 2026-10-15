#include <catch2/catch_test_macros.hpp>

#include "basalt/widget.h"
#include "basalt/widget_tree.h"

#include <memory>
#include <ranges>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace basalt;

namespace {

struct labelled : widget {
  explicit labelled(std::string name) : name(std::move(name)) {}
  std::string name;
};

} // namespace

TEST_CASE("widget ids are valid, unique and hashable", "[widget]") {
  widget a;
  widget b;
  REQUIRE(a.id().valid());
  REQUIRE(a.id() != b.id());
  REQUIRE_FALSE(widget_id{}.valid());
  std::unordered_set<widget_id> ids{a.id(), b.id(), a.id()};
  REQUIRE(ids.size() == 2);
}

TEST_CASE("parents own their children in order", "[widget]") {
  widget parent;
  auto &first = parent.emplace_child<labelled>("first");
  auto &third = parent.add_child(std::make_unique<labelled>("third"));
  auto &second = parent.insert_child(1, std::make_unique<labelled>("second"));
  auto &last = parent.insert_child(99, std::make_unique<labelled>("last"));

  REQUIRE(parent.child_count() == 4);
  REQUIRE(&parent.child(0) == &first);
  REQUIRE(&parent.child(1) == &second);
  REQUIRE(&parent.child(2) == &third);
  REQUIRE(&parent.child(3) == &last);
  REQUIRE(first.parent() == &parent);
  REQUIRE(parent.index_of(third) == 2);
  REQUIRE(parent.is_ancestor_of(third));
  REQUIRE_FALSE(third.is_ancestor_of(parent));
  REQUIRE_FALSE(parent.is_ancestor_of(parent));

  std::vector<std::string> names;
  for (auto &c : parent.children()) {
    names.push_back(static_cast<labelled &>(c).name);
  }
  REQUIRE(names == std::vector<std::string>{"first", "second", "third", "last"});
}

TEST_CASE("null children are rejected", "[widget]") {
  widget parent;
  REQUIRE_THROWS_AS(parent.add_child(std::unique_ptr<widget>{}), std::invalid_argument);
  REQUIRE(parent.child_count() == 0);
}

TEST_CASE("removing a child returns ownership and keeps its id", "[widget]") {
  widget parent;
  widget other;
  auto &child = parent.emplace_child<labelled>("child");
  auto &grandchild = child.emplace_child<widget>();
  auto const id = child.id();

  REQUIRE(other.remove_child(child) == nullptr);

  auto owned = parent.remove_child(child);
  REQUIRE(owned.get() == &child);
  REQUIRE(child.parent() == nullptr);
  REQUIRE(parent.child_count() == 0);
  REQUIRE(grandchild.parent() == &child);

  other.add_child(std::move(owned));
  REQUIRE(child.parent() == &other);
  REQUIRE(child.id() == id);

  REQUIRE(child.remove_from_parent().get() == &child);
  REQUIRE(widget{}.remove_from_parent() == nullptr);
}

TEST_CASE("move_child reorders children", "[widget]") {
  widget parent;
  auto &a = parent.emplace_child<widget>();
  auto &b = parent.emplace_child<widget>();
  auto &c = parent.emplace_child<widget>();

  REQUIRE(parent.move_child(a, 99));
  REQUIRE(parent.index_of(a) == 2);
  REQUIRE(parent.move_child(c, 0));
  REQUIRE(&parent.child(0) == &c);
  REQUIRE(&parent.child(1) == &b);
  REQUIRE(&parent.child(2) == &a);

  widget stranger;
  REQUIRE_FALSE(parent.move_child(stranger, 0));
}

TEST_CASE("bounds are relative to the parent", "[widget][geometry]") {
  widget root;
  root.set_bounds({{10, 20}, {300, 200}});
  auto &panel = root.emplace_child<widget>();
  panel.set_bounds({{5, 5}, {100, 100}});
  auto &button = panel.emplace_child<widget>();
  button.set_bounds({{1, 2}, {40, 40}});

  REQUIRE(button.bounds_in_root() == rect{{16, 27}, {40, 40}});
  REQUIRE(button.to_root({3, 4}) == point{19, 31});
  REQUIRE(button.from_root({19, 31}) == point{3, 4});
}

TEST_CASE("visibility is inherited", "[widget]") {
  widget root;
  auto &panel = root.emplace_child<widget>();
  auto &leaf = panel.emplace_child<widget>();
  REQUIRE(leaf.visible_in_tree());
  panel.set_visible(false);
  REQUIRE(leaf.visible());
  REQUIRE_FALSE(leaf.visible_in_tree());
}

TEST_CASE("layout invalidation marks the widget and its ancestors", "[widget][layout]") {
  widget root;
  auto &panel = root.emplace_child<widget>();
  auto &leaf = panel.emplace_child<widget>();
  REQUIRE(leaf.needs_layout());
  REQUIRE(root.subtree_needs_layout());
  REQUIRE_FALSE(leaf.is_attached());
  REQUIRE(leaf.state() == widget_state::detached);
}

namespace {

// Counts how often it is asked for its size.
struct counting : widget {
  explicit counting(size wanted = {10, 10}) : wanted(wanted) {}

  size wanted;
  mutable int measures = 0;

  auto change() -> void {
    wanted.height += 1;
    invalidate_layout();
  }

protected:
  [[nodiscard]] auto on_measure(constraints const &) const -> size override {
    ++measures;
    return wanted;
  }
};

} // namespace

TEST_CASE("measuring remembers the answer until layout is invalidated", "[widget][layout]") {
  counting w;
  constraints const a = constraints::loose({100, 100});
  constraints const b = constraints::loose({50, 100});

  REQUIRE(w.measure(a) == size{10, 10});
  REQUIRE(w.measure(a) == size{10, 10});
  REQUIRE(w.measures == 1);

  SECTION("each set of limits has its own answer") {
    REQUIRE(w.measure(b) == size{10, 10});
    REQUIRE(w.measures == 2);
    (void)w.measure(a);
    (void)w.measure(b);
    REQUIRE(w.measures == 2);
  }

  SECTION("the answer is still clamped to the limits") {
    REQUIRE(w.measure(constraints::tight({30, 30})) == size{30, 30});
  }

  SECTION("invalidating layout asks again") {
    w.change();
    REQUIRE(w.measure(a) == size{10, 11});
    REQUIRE(w.measures == 2);
  }
}

TEST_CASE("a descendant's change drops its ancestors' answers", "[widget][layout]") {
  counting parent;
  auto &child = parent.emplace_child<counting>();
  constraints const limits = constraints::loose({100, 100});
  (void)parent.measure(limits);
  (void)child.measure(limits);
  REQUIRE(parent.measures == 1);

  child.change();
  (void)parent.measure(limits);
  REQUIRE(parent.measures == 2);
  REQUIRE(child.measure(limits) == size{10, 11});
}

TEST_CASE("joining a tree drops answers measured while detached", "[widget][layout]") {
  counting w;
  constraints const limits = constraints::loose({100, 100});
  (void)w.measure(limits);
  REQUIRE(w.measures == 1);

  widget_tree tree;
  tree.set_viewport({200, 200});
  auto &root = tree.emplace_root<widget>();
  auto &added = root.emplace_child<counting>();
  (void)added.measure(limits);
  REQUIRE(added.measures == 1);
  tree.set_scale({2, 1, 1});
  (void)added.measure(limits);
  REQUIRE(added.measures == 2);
}

namespace {

struct derived_widget : widget {};

} // namespace

TEST_CASE("theme is the tree's theme, or the default while detached", "[widget][theme]") {
  widget detached;
  REQUIRE(detached.theme() == basalt::theme{});

  auto const dark = make_theme(color_scheme::dark);
  widget_tree tree{dark};
  auto &root = tree.emplace_root<widget>();
  auto &child = root.emplace_child<widget>();
  REQUIRE(root.theme() == dark);
  REQUIRE(child.theme() == dark);
  REQUIRE(&child.theme() == &tree.theme());
}

TEST_CASE("find_ancestor returns the nearest ancestor of the type", "[widget]") {
  derived_widget outer;
  auto &inner = outer.emplace_child<derived_widget>();
  auto &plain = inner.emplace_child<widget>();
  auto &leaf = plain.emplace_child<widget>();
  REQUIRE(leaf.find_ancestor<derived_widget>() == &inner);
  REQUIRE(inner.find_ancestor<derived_widget>() == &outer);
  REQUIRE(outer.find_ancestor<derived_widget>() == nullptr);
  REQUIRE(leaf.parent_as<derived_widget>() == nullptr);
  REQUIRE(plain.parent_as<derived_widget>() == &inner);
}

TEST_CASE("invalidate_content requests layout and repaint", "[widget][layout]") {
  widget_tree tree;
  tree.set_viewport({100, 100});
  auto &root = tree.emplace_root<widget>();
  auto &leaf = root.emplace_child<widget>();
  tree.update_layout();
  (void)tree.take_damage();
  REQUIRE_FALSE(leaf.needs_layout());
  REQUIRE(tree.damage().empty());

  leaf.invalidate_content();
  REQUIRE(leaf.needs_layout());
  REQUIRE(root.subtree_needs_layout());
}

TEST_CASE("by_state picks the value for the control state", "[interaction]") {
  using enum control_state;
  REQUIRE(by_state(normal, 1, 2, 3) == 1);
  REQUIRE(by_state(hovered, 1, 2, 3) == 2);
  REQUIRE(by_state(pressed, 1, 2, 3) == 3);
  REQUIRE(by_state(disabled, 1, 2, 3) == 1);
}
