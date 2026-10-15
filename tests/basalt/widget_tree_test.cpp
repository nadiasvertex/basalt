#include <catch2/catch_test_macros.hpp>

#include "basalt/widget_tree.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace basalt;

namespace {

using event_log = std::vector<std::string>;

// Records lifecycle hooks and runs optional callbacks from them.
struct probe : widget {
  probe(event_log &log, std::string name) : log(&log), name(std::move(name)) {}

  event_log *log;
  std::string name;
  std::function<void(probe &)> attached;
  std::function<void(probe &)> detached;
  std::function<void(probe &)> layout;
  int layouts = 0;
  int environment_changes = 0;

protected:
  auto on_attached() -> void override {
    log->push_back("attach " + name);
    REQUIRE(tree() != nullptr);
    REQUIRE(tree()->find(id()) == this);
    if (attached) {
      attached(*this);
    }
  }
  auto on_detached() -> void override {
    log->push_back("detach " + name);
    REQUIRE(tree()->find(id()) == this);
    if (detached) {
      detached(*this);
    }
  }
  auto on_layout() -> void override {
    ++layouts;
    log->push_back("layout " + name);
    if (layout) {
      layout(*this);
    }
  }
  auto on_environment_changed() -> void override { ++environment_changes; }
};

// Stacks children vertically, each 10 px tall and as wide as the parent.
auto stack_children(probe &p) -> void {
  float y = 0;
  for (auto &c : p.children()) {
    c.set_bounds({{0, y}, {p.bounds().width(), 10}});
    y += 10;
  }
}

auto make_probe(event_log &log, std::string name) -> std::unique_ptr<probe> {
  return std::make_unique<probe>(log, std::move(name));
}

} // namespace

TEST_CASE("attaching registers the subtree and runs hooks parent first", "[widget_tree]") {
  event_log log;
  auto root = make_probe(log, "root");
  auto &a = root->add_child(make_probe(log, "a"));
  auto &b = a.add_child(make_probe(log, "b"));

  widget_tree tree;
  tree.set_viewport({200, 100});
  auto &r = tree.set_root(std::move(root));

  REQUIRE(log == event_log{"attach root", "attach a", "attach b"});
  REQUIRE(tree.root() == &r);
  REQUIRE(tree.size() == 3);
  REQUIRE(tree.find(b.id()) == &b);
  REQUIRE(b.tree() == &tree);
  REQUIRE(b.state() == widget_state::attached);
  REQUIRE(r.bounds() == rect{{}, {200, 100}});
}

TEST_CASE("removing a subtree runs hooks children first and unregisters it", "[widget_tree]") {
  event_log log;
  widget_tree tree;
  auto &root = tree.set_root(make_probe(log, "root"));
  auto &a = root.add_child(make_probe(log, "a"));
  auto &b = a.add_child(make_probe(log, "b"));
  auto const b_id = b.id();
  log.clear();

  auto owned = root.remove_child(a);
  REQUIRE(log == event_log{"detach b", "detach a"});
  REQUIRE(tree.size() == 1);
  REQUIRE_FALSE(tree.contains(b_id));
  REQUIRE_FALSE(a.is_attached());
  REQUIRE(b.state() == widget_state::detached);

  log.clear();
  root.add_child(std::move(owned));
  REQUIRE(log == event_log{"attach a", "attach b"});
  REQUIRE(tree.find(b_id) == &b);
}

TEST_CASE("reparenting inside a tree keeps ids stable", "[widget_tree]") {
  widget_tree tree;
  auto &root = tree.emplace_root<widget>();
  auto &left = root.emplace_child<widget>();
  auto &right = root.emplace_child<widget>();
  auto &leaf = left.emplace_child<widget>();
  auto const id = leaf.id();

  right.add_child(left.remove_child(leaf));
  REQUIRE(tree.find(id) == &leaf);
  REQUIRE(leaf.parent() == &right);
}

TEST_CASE("the tree detaches its root when replaced, taken or destroyed", "[widget_tree]") {
  event_log log;
  {
    widget_tree tree;
    tree.set_root(make_probe(log, "first"));
    tree.set_root(make_probe(log, "second"));
    REQUIRE(log == event_log{"attach first", "detach first", "attach second"});

    log.clear();
    auto taken = tree.take_root();
    REQUIRE(log == event_log{"detach second"});
    REQUIRE(tree.root() == nullptr);
    REQUIRE(tree.size() == 0);
    REQUIRE(tree.take_root() == nullptr);

    log.clear();
    tree.set_root(std::move(taken));
  }
  REQUIRE(log == event_log{"attach second", "detach second"});
}

TEST_CASE("hooks may add and remove widgets", "[widget_tree][lifecycle]") {
  event_log log;
  widget_tree tree;
  auto root = make_probe(log, "root");
  auto &doomed = root->add_child(make_probe(log, "doomed"));
  std::unique_ptr<widget> removed;
  root->attached = [&](probe &self) {
    self.add_child(make_probe(log, "added"));
    removed = self.remove_child(doomed);
  };
  tree.set_root(std::move(root));

  // `doomed` was removed before its own hook ran, so it saw neither hook.
  REQUIRE(log == event_log{"attach root", "attach added"});
  REQUIRE(removed.get() == &doomed);
  REQUIRE(tree.size() == 2);
}

TEST_CASE("widgets being detached cannot be removed again", "[widget_tree][lifecycle]") {
  event_log log;
  widget_tree tree;
  auto &root = tree.set_root(make_probe(log, "root"));
  auto &a = root.add_child(make_probe(log, "a"));
  auto &b = a.add_child(make_probe(log, "b"));
  std::unique_ptr<widget> stolen;
  b.detached = [&](probe &self) {
    stolen = a.remove_child(self);
    REQUIRE(self.state() == widget_state::detaching);
    REQUIRE_FALSE(tree.destroy_later(a));
  };

  auto owned = root.remove_child(a);
  REQUIRE(owned);
  REQUIRE(stolen == nullptr);
  REQUIRE(a.child_count() == 1);
}

TEST_CASE("destroy_later lets a widget remove itself", "[widget_tree][lifecycle]") {
  event_log log;
  widget_tree tree;
  auto &root = tree.set_root(make_probe(log, "root"));
  auto &self = root.add_child(make_probe(log, "self"));
  auto const id = self.id();

  REQUIRE(tree.destroy_later(self));
  REQUIRE_FALSE(tree.contains(id));
  REQUIRE(root.child_count() == 0);
  REQUIRE(self.name == "self"); // still alive until collected
  tree.collect_garbage();

  widget stranger;
  REQUIRE_FALSE(tree.destroy_later(stranger));
  REQUIRE(tree.destroy_later(root));
  REQUIRE(tree.root() == nullptr);
}

TEST_CASE("update_layout visits invalidated widgets top-down", "[widget_tree][layout]") {
  event_log log;
  widget_tree tree;
  tree.set_viewport({100, 100});
  auto &root = tree.set_root(make_probe(log, "root"));
  root.layout = stack_children;
  auto &a = root.add_child(make_probe(log, "a"));
  auto &b = root.add_child(make_probe(log, "b"));
  auto &leaf = a.add_child(make_probe(log, "leaf"));
  log.clear();

  REQUIRE(tree.needs_layout());
  REQUIRE(tree.update_layout());
  REQUIRE(log == event_log{"layout root", "layout a", "layout leaf", "layout b"});
  REQUIRE(b.bounds() == rect{{0, 10}, {100, 10}});
  REQUIRE_FALSE(tree.needs_layout());

  SECTION("a leaf invalidation re-lays out its ancestors only") {
    log.clear();
    leaf.invalidate_layout();
    REQUIRE(tree.update_layout());
    REQUIRE(log == event_log{"layout root", "layout a", "layout leaf"});
  }

  SECTION("resizing a widget lays it out without its parent") {
    log.clear();
    a.set_bounds({{0, 0}, {50, 50}});
    REQUIRE(tree.update_layout());
    REQUIRE(log == event_log{"layout a"});
  }

  SECTION("moving a widget does not need layout") {
    a.set_bounds({{5, 5}, a.bounds().extent});
    REQUIRE_FALSE(tree.needs_layout());
  }

  SECTION("a widget invalidating an ancestor is laid out in a later pass") {
    log.clear();
    int remaining = 1;
    leaf.layout = [&](probe &) {
      if (remaining-- > 0) {
        root.invalidate_layout();
      }
    };
    leaf.invalidate_layout();
    REQUIRE(tree.update_layout());
    REQUIRE(log == event_log{"layout root", "layout a", "layout leaf", "layout root"});
  }

  SECTION("endless invalidation gives up after the pass limit") {
    root.layout = [](probe &self) { self.invalidate_layout(); };
    root.invalidate_layout();
    REQUIRE_FALSE(tree.update_layout());
    REQUIRE(root.layouts == 1 + static_cast<int>(widget_tree::max_layout_passes));
    REQUIRE(tree.needs_layout());
  }
}

TEST_CASE("paint invalidation accumulates damage in root coordinates", "[widget_tree][paint]") {
  widget_tree tree;
  tree.set_viewport({100, 100});
  auto &root = tree.emplace_root<widget>();
  auto &panel = root.emplace_child<widget>();
  panel.set_bounds({{10, 10}, {50, 50}});
  auto &button = panel.emplace_child<widget>();
  button.set_bounds({{5, 5}, {20, 10}});
  tree.update_layout();
  (void)tree.take_damage();

  button.invalidate_paint();
  REQUIRE(tree.damage().size() == 1);
  REQUIRE(tree.damage()[0] == rect{{15, 15}, {20, 10}});

  SECTION("contained rects are dropped") {
    button.invalidate_paint({{0, 0}, {5, 5}});
    REQUIRE(tree.damage().size() == 1);
    panel.invalidate_paint();
    REQUIRE(tree.damage().size() == 1);
    REQUIRE(tree.damage()[0] == rect{{10, 10}, {50, 50}});
  }

  SECTION("damage is clipped to the viewport") {
    panel.set_bounds({{90, 90}, {50, 50}});
    auto const damage = tree.take_damage();
    REQUIRE(damage.size() == 2);
    REQUIRE(damage[1] == rect{{90, 90}, {10, 10}});
    REQUIRE(tree.damage().empty());
  }

  SECTION("damage is clipped to every ancestor") {
    button.set_bounds({{40, 40}, {20, 20}});
    (void)tree.take_damage();
    button.invalidate_paint();
    REQUIRE(tree.damage().size() == 1);
    REQUIRE(tree.damage()[0] == rect{{50, 50}, {10, 10}});
    (void)tree.take_damage();
    button.set_bounds({{60, 60}, {20, 20}});
    REQUIRE(tree.damage().size() == 1);
    REQUIRE(tree.damage()[0] == rect{{50, 50}, {10, 10}});
    (void)tree.take_damage();
    button.invalidate_paint();
    REQUIRE(tree.damage().empty());
  }

  SECTION("hidden widgets do not damage, but hiding does") {
    (void)tree.take_damage();
    panel.set_visible(false);
    REQUIRE(tree.damage().size() == 1);
    (void)tree.take_damage();
    button.invalidate_paint();
    REQUIRE(tree.damage().empty());
  }

  SECTION("removing a widget damages where it was") {
    (void)tree.take_damage();
    auto owned = panel.remove_child(button);
    REQUIRE(tree.damage().size() == 1);
    REQUIRE(tree.damage()[0] == rect{{15, 15}, {20, 10}});
    (void)tree.take_damage();
    owned->invalidate_paint();
    REQUIRE(tree.damage().empty());
  }

  SECTION("many rects collapse into their bounding rect") {
    (void)tree.take_damage();
    for (int i = 0; i <= static_cast<int>(widget_tree::max_damage_rects); ++i) {
      root.invalidate_paint({{static_cast<float>(i * 5), 0}, {1, 1}});
    }
    REQUIRE(tree.damage().size() == 1);
    REQUIRE(tree.damage()[0] == rect{{0, 0}, {81, 1}});
  }
}

TEST_CASE("the tree requests one frame per batch of changes", "[widget_tree][paint]") {
  widget_tree tree;
  tree.set_viewport({100, 100});
  int requests = 0;
  tree.set_frame_request_handler([&] { ++requests; });
  REQUIRE(requests == 0);

  auto &root = tree.emplace_root<widget>();
  auto &child = root.emplace_child<widget>();
  child.set_bounds({{0, 0}, {10, 10}});
  REQUIRE(requests == 1);
  REQUIRE(tree.needs_frame());

  tree.update_layout();
  (void)tree.take_damage();
  REQUIRE_FALSE(tree.needs_frame());
  child.invalidate_paint();
  child.invalidate_layout();
  REQUIRE(requests == 2);

  SECTION("a frame that leaves layout pending requests another") {
    (void)tree.take_damage();
    REQUIRE(requests == 3);
  }

  SECTION("a new handler is told about pending work") {
    int other = 0;
    tree.set_frame_request_handler([&] { ++other; });
    REQUIRE(other == 1);
  }
}

TEST_CASE("viewport changes resize the root", "[widget_tree]") {
  event_log log;
  widget_tree tree;
  auto &root = tree.set_root(make_probe(log, "root"));
  tree.update_layout();
  log.clear();

  tree.set_viewport({640, 480});
  REQUIRE(root.bounds() == rect{{}, {640, 480}});
  REQUIRE(tree.damage().size() == 1);
  REQUIRE(tree.damage()[0] == rect{{}, {640, 480}});
  tree.update_layout();
  REQUIRE(log == event_log{"layout root"});
}

TEST_CASE("theme and scale changes relayout and repaint everything", "[widget_tree][theme]") {
  event_log log;
  widget_tree tree{make_theme(color_scheme::light), {.display = 2}};
  REQUIRE(tree.scale().display == 2);
  tree.set_viewport({100, 100});
  auto &root = tree.set_root(make_probe(log, "root"));
  auto &child = root.add_child(make_probe(log, "child"));
  tree.update_layout();
  (void)tree.take_damage();
  log.clear();

  tree.set_theme(make_theme(color_scheme::light));
  REQUIRE_FALSE(tree.needs_frame());

  tree.set_theme(make_theme(color_scheme::dark));
  REQUIRE(tree.theme().scheme == color_scheme::dark);
  REQUIRE(root.environment_changes == 1);
  REQUIRE(child.environment_changes == 1);
  REQUIRE(tree.damage()[0] == rect{{}, {100, 100}});
  tree.update_layout();
  REQUIRE(log == event_log{"layout root", "layout child"});

  REQUIRE(tree.set_scale({.display = 2, .zoom = 1, .text = 2}));
  REQUIRE(child.environment_changes == 2);
  REQUIRE(tree.set_scale(tree.scale()));
  REQUIRE(child.environment_changes == 2);

  REQUIRE_FALSE(tree.set_scale({.display = 0}));
  REQUIRE(tree.scale().text == 2);
}

TEST_CASE("clear_children removes every child, and a tree keeps them alive until collected",
          "[widget_tree][lifecycle]") {
  event_log log;
  widget_tree tree;
  auto &root = tree.set_root(make_probe(log, "root"));
  auto &first = root.add_child(make_probe(log, "first"));
  root.add_child(make_probe(log, "second"));
  auto const id = first.id();

  root.clear_children();
  CHECK(root.child_count() == 0);
  CHECK_FALSE(tree.contains(id));
  CHECK(first.name == "first");
  tree.collect_garbage();

  widget loose;
  loose.add_child(std::make_unique<widget>());
  loose.clear_children();
  CHECK(loose.child_count() == 0);
}
