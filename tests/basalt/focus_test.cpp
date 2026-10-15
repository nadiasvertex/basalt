#include <catch2/catch_test_macros.hpp>

#include "basalt/focus.h"
#include "basalt/widget_tree.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "input_probe.h"

using namespace basalt;
using basalt::testing::input_probe;

namespace {

auto make(std::string name, focus_policy policy = focus_policy::none, rect area = {})
    -> std::unique_ptr<input_probe> {
  auto w = std::make_unique<input_probe>(std::move(name), area);
  w->set_focus_policy(policy);
  return w;
}

struct fixture {
  fixture() {
    tree.set_viewport({400, 300});
    root = &tree.set_root(make("root"));
  }

  [[nodiscard]] auto focus() -> focus_manager & { return tree.focus(); }

  widget_tree tree;
  input_probe *root;
};

auto encloses(rect outer, rect inner) -> bool {
  return inner.left() >= outer.left() && inner.top() >= outer.top() &&
         inner.right() <= outer.right() && inner.bottom() <= outer.bottom();
}

} // namespace

TEST_CASE("Tab order follows the tree and the focus policies", "[focus]") {
  fixture f;
  auto &a = f.root->add_child(make("a", focus_policy::strong));
  auto &hidden = f.root->add_child(make("hidden", focus_policy::tab));
  auto &click = f.root->add_child(make("click", focus_policy::click));
  auto &box = f.root->add_child(make("box"));
  auto &inner = box.add_child(make("inner", focus_policy::tab));
  auto &disabled = f.root->add_child(make("disabled", focus_policy::tab));
  auto &hidden_child = hidden.add_child(make("hidden child", focus_policy::tab));
  hidden.set_visible(false);
  disabled.set_enabled(false);

  REQUIRE(f.focus().tab_order() == std::vector<widget *>{&a, &inner, &disabled});
  REQUIRE(f.focus().tab_order(&box) == std::vector<widget *>{&inner});

  REQUIRE(f.focus().move_focus(focus_move::next));
  REQUIRE(a.has_focus());
  REQUIRE(f.focus().move_focus(focus_move::next));
  REQUIRE(inner.has_focus());
  REQUIRE(f.focus().move_focus(focus_move::last));
  REQUIRE(disabled.has_focus());
  REQUIRE(f.focus().move_focus(focus_move::next));
  REQUIRE(a.has_focus());
  REQUIRE(f.focus().move_focus(focus_move::previous));
  REQUIRE(disabled.has_focus());
  REQUIRE(f.focus().move_focus(focus_move::first));
  REQUIRE(a.has_focus());

  SECTION("from no focus, previous goes to the last stop") {
    f.focus().clear_focus();
    REQUIRE(f.focus().move_focus(focus_move::previous));
    REQUIRE(disabled.has_focus());
  }

  SECTION("from a widget that is not a Tab stop, moves go to its neighbours") {
    REQUIRE(click.request_focus());
    REQUIRE(f.focus().move_focus(focus_move::next));
    REQUIRE(inner.has_focus());
    REQUIRE(click.request_focus());
    REQUIRE(f.focus().move_focus(focus_move::previous));
    REQUIRE(a.has_focus());
  }

  SECTION("hidden and policy-less widgets refuse focus") {
    REQUIRE_FALSE(hidden_child.request_focus());
    REQUIRE_FALSE(box.request_focus());
    REQUIRE(a.has_focus());
  }

  SECTION("an empty tree has no stops") {
    widget_tree empty;
    REQUIRE_FALSE(empty.focus().move_focus(focus_move::next));
    REQUIRE(empty.focus().tab_order().empty());
  }
}

TEST_CASE("a group scope is one Tab stop that remembers its entry", "[focus]") {
  fixture f;
  auto &before = f.root->add_child(make("before", focus_policy::tab));
  auto &toolbar = f.root->add_child(make("toolbar"));
  toolbar.set_focus_scope(focus_scope::group);
  auto &t1 = toolbar.add_child(make("t1", focus_policy::tab));
  auto &t2 = toolbar.add_child(make("t2", focus_policy::tab));
  auto &t3 = toolbar.add_child(make("t3", focus_policy::tab));
  auto &after = f.root->add_child(make("after", focus_policy::tab));

  REQUIRE(f.focus().tab_order() == std::vector<widget *>{&before, &t1, &after});

  REQUIRE(t2.request_focus());
  REQUIRE(f.focus().tab_order() == std::vector<widget *>{&before, &t2, &after});
  REQUIRE(f.focus().move_focus(focus_move::next));
  REQUIRE(after.has_focus());
  REQUIRE(f.focus().move_focus(focus_move::previous));
  REQUIRE(t2.has_focus());

  REQUIRE(f.focus().move_focus_in_group(focus_move::next));
  REQUIRE(t3.has_focus());
  REQUIRE_FALSE(f.focus().move_focus_in_group(focus_move::next, false));
  REQUIRE(t3.has_focus());
  REQUIRE(f.focus().move_focus_in_group(focus_move::next));
  REQUIRE(t1.has_focus());
  REQUIRE(f.focus().move_focus_in_group(focus_move::last));
  REQUIRE(t3.has_focus());
  REQUIRE(f.focus().move_focus_in_group(focus_move::previous));
  REQUIRE(t2.has_focus());

  SECTION("the entry can be set") {
    REQUIRE(f.focus().set_group_entry(toolbar, t3));
    REQUIRE(f.focus().tab_order() == std::vector<widget *>{&before, &t3, &after});
    REQUIRE_FALSE(f.focus().set_group_entry(*f.root, t3));
    REQUIRE_FALSE(f.focus().set_group_entry(toolbar, after));
  }

  SECTION("an entry that can no longer take focus falls back to the first stop") {
    t2.set_visible(false);
    REQUIRE(f.focus().tab_order() == std::vector<widget *>{&before, &t1, &after});
  }

  SECTION("moving in a group needs focus inside one") {
    REQUIRE(before.request_focus());
    REQUIRE_FALSE(f.focus().move_focus_in_group(focus_move::next));
  }

  SECTION("a group without stops is skipped") {
    for (auto *t : {&t1, &t2, &t3}) {
      t->set_focus_policy(focus_policy::none);
    }
    REQUIRE(f.focus().tab_order() == std::vector<widget *>{&before, &after});
  }
}

TEST_CASE("focus flags mark the focused widget and its ancestors", "[focus]") {
  fixture f;
  auto &box = f.root->add_child(make("box"));
  auto &field = box.add_child(make("field", focus_policy::strong));
  auto &other = f.root->add_child(make("other", focus_policy::strong));

  std::vector<widget *> changes;
  f.focus().set_focus_changed_handler([&](widget *w) { changes.push_back(w); });

  REQUIRE(field.request_focus());
  REQUIRE(f.focus().focused() == &field);
  REQUIRE(field.interaction().focused);
  REQUIRE(field.interaction().focus_within);
  REQUIRE(box.interaction().focus_within);
  REQUIRE_FALSE(box.interaction().focused);
  REQUIRE(f.root->interaction().focus_within);
  // Programmatic focus keeps the pointer modality the tree starts in.
  REQUIRE_FALSE(field.interaction().focus_visible);

  REQUIRE(other.request_focus(focus_reason::keyboard));
  REQUIRE_FALSE(field.interaction().focused);
  REQUIRE_FALSE(box.interaction().focus_within);
  REQUIRE(f.root->interaction().focus_within);
  REQUIRE(other.interaction().focus_visible);
  REQUIRE(f.focus().focus_visible());

  f.focus().set_window_active(false);
  REQUIRE_FALSE(other.interaction().focus_visible);
  REQUIRE(other.interaction().focused);
  f.focus().set_window_active(true);
  REQUIRE(other.interaction().focus_visible);

  REQUIRE(field.request_focus(focus_reason::pointer));
  REQUIRE_FALSE(field.interaction().focus_visible);
  f.focus().set_keyboard_modality(true);
  REQUIRE(field.interaction().focus_visible);

  f.focus().clear_focus();
  REQUIRE(f.focus().focused() == nullptr);
  REQUIRE_FALSE(f.root->interaction().focus_within);
  REQUIRE(changes == std::vector<widget *>{&field, &other, &field, nullptr});
}

TEST_CASE("focus falls back when the focused widget leaves or is hidden", "[focus]") {
  fixture f;
  auto &box = f.root->add_child(make("box", focus_policy::click));
  auto &field = box.add_child(make("field", focus_policy::tab));
  std::vector<widget *> changes;
  f.focus().set_focus_changed_handler([&](widget *w) { changes.push_back(w); });
  REQUIRE(field.request_focus());

  SECTION("removed: the nearest ancestor that accepts focus takes it") {
    auto removed = box.remove_child(field);
    REQUIRE(box.has_focus());
    REQUIRE_FALSE(removed->interaction().focused);
    auto box_owner = f.root->remove_child(box);
    REQUIRE(f.focus().focused() == nullptr);
    REQUIRE_FALSE(f.root->interaction().focus_within);
    REQUIRE(changes == std::vector<widget *>{&field, &box, nullptr});
  }

  SECTION("destroyed with the tree's root") {
    auto old_root = f.tree.take_root();
    REQUIRE(f.focus().focused() == nullptr);
  }

  SECTION("hidden") {
    field.set_visible(false);
    REQUIRE(box.has_focus());
    box.set_visible(false);
    REQUIRE(f.focus().focused() == nullptr);
  }

  SECTION("destroyed later") {
    REQUIRE(f.tree.destroy_later(box));
    REQUIRE(f.focus().focused() == nullptr);
    f.tree.collect_garbage();
  }
}

TEST_CASE("a modal traps focus and restores it when popped", "[focus]") {
  fixture f;
  auto &editor = f.root->add_child(make("editor", focus_policy::strong));
  auto &dialog = f.root->add_child(make("dialog"));
  auto &name = dialog.add_child(make("name", focus_policy::strong));
  auto &ok = dialog.add_child(make("ok", focus_policy::tab));
  REQUIRE(editor.request_focus());

  REQUIRE(f.focus().push_modal(dialog));
  REQUIRE(f.focus().modal_root() == &dialog);
  REQUIRE(name.has_focus());
  REQUIRE(f.focus().is_blocked(editor));
  REQUIRE_FALSE(f.focus().is_blocked(ok));
  REQUIRE_FALSE(f.focus().is_blocked(dialog));
  REQUIRE_FALSE(editor.request_focus());

  REQUIRE(f.focus().tab_order() == std::vector<widget *>{&name, &ok});
  REQUIRE(f.focus().move_focus(focus_move::next));
  REQUIRE(ok.has_focus());
  REQUIRE(f.focus().move_focus(focus_move::next));
  REQUIRE(name.has_focus());

  SECTION("popping restores the earlier focus") {
    REQUIRE(f.focus().pop_modal(dialog));
    REQUIRE(f.focus().modal_root() == nullptr);
    REQUIRE(editor.has_focus());
    REQUIRE_FALSE(f.focus().pop_modal(dialog));
  }

  SECTION("a modal that leaves the tree is popped") {
    auto removed = f.root->remove_child(dialog);
    REQUIRE(f.focus().modal_root() == nullptr);
    REQUIRE(editor.has_focus());
  }

  SECTION("modals nest") {
    auto &confirm = f.root->add_child(make("confirm"));
    auto &yes = confirm.add_child(make("yes", focus_policy::tab));
    REQUIRE(f.focus().push_modal(confirm));
    REQUIRE(yes.has_focus());
    REQUIRE(f.focus().is_blocked(name));
    REQUIRE(f.focus().pop_modal(confirm));
    REQUIRE(name.has_focus());
    REQUIRE(f.focus().modal_root() == &dialog);
  }

  SECTION("a restore target that is gone leaves focus alone") {
    auto removed_editor = f.root->remove_child(editor);
    REQUIRE(f.focus().pop_modal(dialog));
    REQUIRE(name.has_focus());
  }

  SECTION("a modal without stops takes focus itself, or nobody does") {
    auto &notice = f.root->add_child(make("notice", focus_policy::click));
    REQUIRE(f.focus().push_modal(notice));
    REQUIRE(notice.has_focus());
    auto &blank = f.root->add_child(make("blank"));
    REQUIRE(f.focus().push_modal(blank));
    REQUIRE(f.focus().focused() == nullptr);
  }

  SECTION("a detached widget cannot be a modal") {
    input_probe loose;
    REQUIRE_FALSE(f.focus().push_modal(loose));
  }
}

TEST_CASE("the focus ring follows the Cubit focus tokens", "[focus]") {
  fixture f;
  auto &box = f.root->add_child(make("box", focus_policy::none, {{10, 10}, {200, 100}}));
  auto &button = box.add_child(make("button", focus_policy::strong, {{20, 30}, {100, 40}}));
  (void)f.tree.update_layout();
  (void)f.tree.take_damage();

  auto const shape = button.focus_ring();
  REQUIRE(shape);
  REQUIRE(*shape == focus_ring_shape{{{}, {100, 40}}, 4});

  // Ring width 2 and gap 2 put the outer edge 4 px outside the bounds; the
  // corner radius grows by the same amount.
  focus_ring const ring = make_focus_ring(button, *shape);
  REQUIRE(ring.outer == rect{{26, 36}, {108, 48}});
  REQUIRE(ring.width == 2);
  REQUIRE(ring.corner_radius == 8);
  REQUIRE(ring.color == f.tree.theme().colors.border_focus);
  REQUIRE(ring.clip == rect{{10, 10}, {200, 100}});

  REQUIRE_FALSE(visible_focus_ring(f.tree));
  REQUIRE(button.request_focus(focus_reason::pointer));
  REQUIRE_FALSE(visible_focus_ring(f.tree));
  (void)f.tree.take_damage();

  REQUIRE(button.request_focus(focus_reason::keyboard));
  REQUIRE(visible_focus_ring(f.tree) == ring);
  auto const damage = f.tree.take_damage();
  REQUIRE(std::ranges::any_of(damage, [&](rect r) { return encloses(r, ring.outer); }));

  SECTION("moving the focused widget damages the ring at both places") {
    button.set_bounds({{60, 30}, {100, 40}});
    auto const moved = f.tree.take_damage();
    REQUIRE(std::ranges::any_of(moved, [&](rect r) { return encloses(r, ring.outer); }));
    auto const now = visible_focus_ring(f.tree);
    REQUIRE(now);
    REQUIRE(std::ranges::any_of(
        moved, [&](rect r) { return encloses(r, intersection(now->outer, now->clip)); }));
  }

  SECTION("hiding focus damages the ring") {
    f.focus().set_window_active(false);
    REQUIRE_FALSE(visible_focus_ring(f.tree));
    auto const hidden = f.tree.take_damage();
    REQUIRE(std::ranges::any_of(hidden, [&](rect r) { return encloses(r, ring.outer); }));
  }

  SECTION("zoom scales the ring and display scale keeps it whole device pixels") {
    REQUIRE(f.tree.set_scale({.display = 1, .zoom = 2}));
    focus_ring const zoomed = make_focus_ring(button, focus_ring_shape{{{}, {100, 40}}, 0});
    REQUIRE(zoomed.width == 4);
    REQUIRE(zoomed.outer == rect{{22, 32}, {116, 56}});
    REQUIRE(zoomed.corner_radius == 0);

    REQUIRE(f.tree.set_scale({.display = 1.5f}));
    focus_ring const fractional = make_focus_ring(button, focus_ring_shape{{{}, {100, 40}}, 0});
    REQUIRE(fractional.width * 1.5f == 3);
  }

  SECTION("widgets that draw their own focus have no ring") {
    button.custom_ring = true;
    REQUIRE_FALSE(visible_focus_ring(f.tree));
  }
}
