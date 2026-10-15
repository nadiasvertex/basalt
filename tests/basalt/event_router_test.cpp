#include <catch2/catch_test_macros.hpp>

#include "basalt/event_router.h"
#include "basalt/widget_tree.h"

#include <memory>
#include <utility>
#include <variant>

#include "input_probe.h"

using namespace basalt;
using basalt::testing::input_probe;
using basalt::testing::key_press;
using basalt::testing::move;
using basalt::testing::press;
using basalt::testing::release;

namespace {

auto make(std::string name, rect area) -> std::unique_ptr<input_probe> {
  return std::make_unique<input_probe>(std::move(name), area);
}

struct fixture {
  fixture() {
    tree.set_viewport({400, 300});
    root = &tree.set_root(make("root", {}));
  }

  widget_tree tree;
  event_router router{tree};
  input_probe *root;
};

} // namespace

TEST_CASE("hit testing finds the deepest frontmost widget", "[event_router]") {
  fixture f;
  auto &panel = f.root->add_child(make("panel", {{10, 10}, {200, 100}}));
  auto &back = panel.add_child(make("back", {{0, 0}, {100, 50}}));
  auto &front = panel.add_child(make("front", {{50, 0}, {100, 50}}));

  REQUIRE(f.router.hit_test({20, 20}) == &back);
  REQUIRE(f.router.hit_test({70, 20}) == &front);
  REQUIRE(f.router.hit_test({20, 80}) == &panel);
  REQUIRE(f.router.hit_test({300, 200}) == f.root);
  REQUIRE(f.router.hit_test({500, 200}) == nullptr);

  SECTION("children are clipped to their parent") {
    auto &overhang = panel.add_child(make("overhang", {{150, 80}, {100, 100}}));
    REQUIRE(f.router.hit_test({170, 100}) == &overhang);
    REQUIRE(f.router.hit_test({250, 100}) == f.root);
  }

  SECTION("hidden widgets are skipped") {
    front.set_visible(false);
    REQUIRE(f.router.hit_test({70, 20}) == &back);
  }

  SECTION("a widget that is not hit-testable lets the pointer through to what is behind") {
    front.set_hit_testable(false);
    REQUIRE(f.router.hit_test({140, 20}) == &panel);
    auto &inside = front.add_child(make("inside", {{80, 0}, {20, 20}}));
    REQUIRE(f.router.hit_test({140, 15}) == &inside);
  }

  SECTION("disabled widgets pass the pointer to their nearest enabled ancestor") {
    auto &child = front.add_child(make("child", {{0, 0}, {10, 10}}));
    front.set_enabled(false);
    REQUIRE(f.router.hit_test({62, 12}) == &panel);
    REQUIRE_FALSE(child.enabled_in_tree());
  }
}

TEST_CASE("small targets grow to the minimum target size", "[event_router]") {
  fixture f;
  auto &button = f.root->add_child(make("button", {{100, 100}, {16, 16}}));
  button.set_minimum_target(true);

  // 40 px centred on a 16 px control reaches 12 px past each edge.
  REQUIRE(f.router.hit_area(button) == rect{{-12, -12}, {40, 40}});
  REQUIRE(f.router.hit_test({90, 108}) == &button);
  REQUIRE(f.router.hit_test({87, 108}) == f.root);

  SECTION("a widget whose bounds hold the point wins over a grown area") {
    auto &neighbour = f.root->add_child(make("neighbour", {{80, 100}, {15, 16}}));
    REQUIRE(f.router.hit_test({90, 108}) == &neighbour);
  }

  SECTION("the nearest of two grown areas wins") {
    auto &other = f.root->add_child(make("other", {{124, 100}, {16, 16}}));
    other.set_minimum_target(true);
    REQUIRE(f.router.hit_test({119, 108}) == &button);
    REQUIRE(f.router.hit_test({121, 108}) == &other);
  }

  SECTION("the minimum follows zoom") {
    REQUIRE(f.tree.set_scale({.display = 1, .zoom = 2}));
    REQUIRE(f.router.hit_area(button) == rect{{-32, -32}, {80, 80}});
  }

  SECTION("widgets without a minimum target are hit only inside their bounds") {
    button.set_minimum_target(false);
    REQUIRE(f.router.hit_area(button) == rect{{}, {16, 16}});
    REQUIRE(f.router.hit_test({90, 108}) == f.root);
  }
}

TEST_CASE("pointer events bubble with local positions until handled", "[event_router]") {
  fixture f;
  auto &panel = f.root->add_child(make("panel", {{10, 20}, {200, 100}}));
  auto &child = panel.add_child(make("child", {{5, 5}, {50, 50}}));

  REQUIRE_FALSE(f.router.dispatch(move({30, 40})));
  REQUIRE(child.last_pointer().position == point{15, 15});
  REQUIRE(panel.last_pointer().position == point{20, 20});
  REQUIRE(f.root->last_pointer().position == point{30, 40});

  panel.handles_all = true;
  f.root->received.clear();
  REQUIRE(f.router.dispatch(move({31, 40})));
  REQUIRE(f.root->received.empty());

  SECTION("the context names the target and the current receiver") {
    widget *seen_target = nullptr;
    widget *seen_current = nullptr;
    panel.handler = [&](event_context &context, event const &) {
      seen_target = context.target();
      seen_current = &context.current();
      REQUIRE(context.root_position() == point{32, 40});
      return true;
    };
    REQUIRE(f.router.dispatch(move({32, 40})));
    REQUIRE(seen_target == &child);
    REQUIRE(seen_current == &panel);
  }

  SECTION("disabled widgets receive nothing") {
    panel.handles_all = false;
    child.set_enabled(false);
    child.received.clear();
    REQUIRE_FALSE(f.router.dispatch(press({30, 40})));
    REQUIRE(child.received.empty());
    REQUIRE(panel.count(pointer_action::down) == 1);
  }
}

TEST_CASE("a handled primary press captures the pointer", "[event_router]") {
  fixture f;
  auto &button = f.root->add_child(make("button", {{0, 0}, {50, 50}}));
  auto &other = f.root->add_child(make("other", {{100, 0}, {50, 50}}));
  button.handles_all = true;

  REQUIRE(f.router.dispatch(press({10, 10})));
  REQUIRE(f.router.pointer_capture() == &button);

  REQUIRE(f.router.dispatch(move({120, 10}, pointer_button::primary)));
  REQUIRE(button.last_pointer().position == point{120, 10});
  REQUIRE(other.received.empty());
  // Only the captured widget can be hovered while it holds the pointer.
  REQUIRE_FALSE(other.interaction().hovered);
  REQUIRE_FALSE(button.interaction().hovered);

  REQUIRE(f.router.dispatch(release({120, 10})));
  REQUIRE(button.count(pointer_action::up) == 1);
  REQUIRE(f.router.pointer_capture() == nullptr);
  REQUIRE(other.interaction().hovered);

  SECTION("a handler can decline capture") {
    button.handler = [](event_context &context, event const &e) {
      if (std::get<pointer_event>(e).action == pointer_action::down) {
        context.release_pointer();
      }
      return true;
    };
    REQUIRE(f.router.dispatch(press({10, 10})));
    REQUIRE(f.router.pointer_capture() == nullptr);
  }

  SECTION("a secondary press does not capture") {
    REQUIRE(f.router.dispatch(press({10, 10}, pointer_button::secondary)));
    REQUIRE(f.router.pointer_capture() == nullptr);
  }

  SECTION("an unhandled press does not capture") {
    REQUIRE_FALSE(f.router.dispatch(press({120, 10})));
    REQUIRE(f.router.pointer_capture() == nullptr);
  }

  SECTION("explicit capture works from any event") {
    other.handler = [](event_context &context, event const &) {
      context.capture_pointer();
      return false;
    };
    REQUIRE_FALSE(f.router.dispatch(move({120, 10})));
    REQUIRE(f.router.pointer_capture() == &other);
  }
}

TEST_CASE("capture ends when the widget leaves, is disabled or the window loses focus",
          "[event_router]") {
  fixture f;
  auto &button = f.root->add_child(make("button", {{0, 0}, {50, 50}}));
  button.handles_all = true;
  REQUIRE(f.router.dispatch(press({10, 10})));
  REQUIRE(f.router.pointer_capture() == &button);

  SECTION("removed") {
    auto const removed = f.root->remove_child(button);
    REQUIRE(f.router.pointer_capture() == nullptr);
    REQUIRE_FALSE(f.router.dispatch(move({10, 10}, pointer_button::primary)));
    REQUIRE(f.root->count(pointer_action::move) == 1);
  }

  SECTION("disabled") {
    button.set_enabled(false);
    (void)f.router.dispatch(move({10, 10}, pointer_button::primary));
    REQUIRE(f.router.pointer_capture() == nullptr);
  }

  SECTION("window focus lost") {
    REQUIRE(f.router.dispatch(window_event{.action = window_action::focus_lost}));
    REQUIRE(button.count(pointer_action::cancel) == 1);
    REQUIRE(f.router.pointer_capture() == nullptr);
    REQUIRE_FALSE(button.interaction().pressed);
  }

  SECTION("platform cancel") {
    REQUIRE(f.router.dispatch(pointer_event{.action = pointer_action::cancel}));
    REQUIRE(button.count(pointer_action::cancel) == 1);
    REQUIRE(f.router.pointer_capture() == nullptr);
  }
}

TEST_CASE("hover and press flags follow the pointer", "[event_router]") {
  fixture f;
  auto &panel = f.root->add_child(make("panel", {{0, 0}, {200, 100}}));
  auto &button = panel.add_child(make("button", {{10, 10}, {50, 30}}));
  auto &label = button.add_child(make("label", {{5, 5}, {20, 10}}));
  button.handles_all = true;

  (void)f.router.dispatch(move({20, 20}));
  REQUIRE(label.interaction().hovered);
  REQUIRE(button.interaction().hovered);
  REQUIRE(panel.interaction().hovered);
  REQUIRE(f.router.hovered() == &label);

  (void)f.router.dispatch(move({40, 30}));
  REQUIRE_FALSE(label.interaction().hovered);
  REQUIRE(button.interaction().hovered);
  REQUIRE(control_state_of(button.interaction(), button.enabled_in_tree()) ==
          control_state::hovered);

  (void)f.router.dispatch(press({40, 30}));
  REQUIRE(button.interaction().pressed);
  REQUIRE(f.router.pressed() == &button);
  REQUIRE(control_state_of(button.interaction(), true) == control_state::pressed);

  // Dragging off the pressed widget shows it unpressed until the pointer returns.
  (void)f.router.dispatch(move({150, 80}, pointer_button::primary));
  REQUIRE_FALSE(button.interaction().pressed);
  REQUIRE(f.router.pressed() == nullptr);
  (void)f.router.dispatch(move({41, 30}, pointer_button::primary));
  REQUIRE(button.interaction().pressed);

  (void)f.router.dispatch(release({41, 30}));
  REQUIRE_FALSE(button.interaction().pressed);
  REQUIRE(button.interaction().hovered);

  (void)f.router.dispatch(pointer_event{.action = pointer_action::leave});
  REQUIRE_FALSE(button.interaction().hovered);
  REQUIRE_FALSE(panel.interaction().hovered);
  REQUIRE(f.router.hovered() == nullptr);

  SECTION("hover is recomputed after widgets move") {
    (void)f.router.dispatch(move({20, 20}));
    REQUIRE(label.interaction().hovered);
    button.set_bounds({{100, 50}, {50, 30}});
    f.router.update_hover();
    REQUIRE_FALSE(button.interaction().hovered);
    REQUIRE(panel.interaction().hovered);
  }

  SECTION("flag changes reach the widget") {
    button.transitions.clear();
    (void)f.router.dispatch(move({40, 30}));
    REQUIRE(button.transitions.size() == 1);
    REQUIRE_FALSE(button.transitions.front().hovered);
  }

  SECTION("flags are cleared when a widget leaves the tree") {
    (void)f.router.dispatch(move({40, 30}));
    auto removed = panel.remove_child(button);
    REQUIRE(removed->interaction() == interaction_state{});
  }
}

TEST_CASE("control states have a fixed precedence", "[event_router]") {
  interaction_state s;
  REQUIRE(control_state_of(s, true) == control_state::normal);
  s.hovered = true;
  REQUIRE(control_state_of(s, true) == control_state::hovered);
  s.pressed = true;
  REQUIRE(control_state_of(s, true) == control_state::pressed);
  REQUIRE(control_state_of(s, false) == control_state::disabled);
}

TEST_CASE("wheel events go to the widget under the pointer", "[event_router]") {
  fixture f;
  auto &list = f.root->add_child(make("list", {{0, 0}, {100, 100}}));
  auto &other = f.root->add_child(make("other", {{200, 0}, {100, 100}}));
  list.handles_all = true;
  other.handles_all = true;

  REQUIRE(f.router.dispatch(press({10, 10})));
  REQUIRE(f.router.dispatch(wheel_event{.position = {210, 10}, .delta = {0, 1}}));
  REQUIRE(other.received.size() == 1);
  REQUIRE(std::get<wheel_event>(other.received.back()).position == point{10, 10});
}

TEST_CASE("keyboard events go to the focused widget and bubble", "[event_router]") {
  fixture f;
  auto &form = f.root->add_child(make("form", {{0, 0}, {200, 200}}));
  auto &first = form.add_child(make("first", {{0, 0}, {100, 30}}));
  auto &second = form.add_child(make("second", {{0, 40}, {100, 30}}));
  first.set_focus_policy(focus_policy::strong);
  second.set_focus_policy(focus_policy::strong);

  // With nothing focused the root receives them.
  REQUIRE_FALSE(f.router.dispatch(text_input_event{.text = "a"}));
  REQUIRE(f.root->received.size() == 1);
  f.root->received.clear();

  REQUIRE(first.request_focus());
  REQUIRE_FALSE(f.router.dispatch(key_press(key::a)));
  REQUIRE(first.received.size() == 1);
  REQUIRE(form.received.size() == 1);
  REQUIRE(f.root->received.size() == 1);

  SECTION("unhandled Tab moves focus") {
    REQUIRE(f.router.dispatch(key_press(key::tab)));
    REQUIRE(second.has_focus());
    REQUIRE(f.router.dispatch(key_press(key::tab, modifier::shift)));
    REQUIRE(first.has_focus());
    REQUIRE_FALSE(f.router.dispatch(key_press(key::tab, modifier::control)));
    REQUIRE(first.has_focus());
  }

  SECTION("a widget can consume Tab") {
    first.handles_all = true;
    REQUIRE(f.router.dispatch(key_press(key::tab)));
    REQUIRE(first.has_focus());
  }

  SECTION("commands go to the focused widget too") {
    form.handles_all = true;
    REQUIRE(f.router.dispatch(command_event{.id = "edit.copy"}));
    REQUIRE(std::holds_alternative<command_event>(form.received.back()));
  }
}

TEST_CASE("pressing focuses the innermost click-focusable widget", "[event_router]") {
  fixture f;
  auto &editor = f.root->add_child(make("editor", {{0, 0}, {200, 200}}));
  auto &text = editor.add_child(make("text", {{10, 10}, {50, 20}}));
  auto &tool = editor.add_child(make("tool", {{100, 10}, {50, 20}}));
  editor.set_focus_policy(focus_policy::strong);
  tool.set_focus_policy(focus_policy::tab);

  REQUIRE_FALSE(f.router.dispatch(press({20, 20})));
  REQUIRE(editor.has_focus());
  REQUIRE_FALSE(editor.interaction().focus_visible);
  (void)f.router.dispatch(release({20, 20}));
  REQUIRE(text.received.size() == 2);

  // A Tab-only control does not take focus by pointer, and neither does its
  // focusable ancestor.
  REQUIRE(f.router.dispatch(key_press(key::tab)));
  REQUIRE(tool.has_focus());
  REQUIRE(tool.interaction().focus_visible);
  (void)f.router.dispatch(press({120, 20}));
  REQUIRE(tool.has_focus());
  REQUIRE_FALSE(tool.interaction().focus_visible);

  // Pressing inert content keeps focus where it is.
  (void)f.router.dispatch(press({300, 250}));
  REQUIRE(tool.has_focus());

  // Any key press makes focus visible again; a lone modifier does not.
  (void)f.router.dispatch(key_press(key::left_shift));
  REQUIRE_FALSE(tool.interaction().focus_visible);
  (void)f.router.dispatch(key_press(key::down));
  REQUIRE(tool.interaction().focus_visible);
}

TEST_CASE("a modal blocks pointer input outside it and stops keyboard bubbling", "[event_router]") {
  fixture f;
  auto &behind = f.root->add_child(make("behind", {{0, 0}, {100, 100}}));
  auto &dialog = f.root->add_child(make("dialog", {{150, 50}, {200, 150}}));
  auto &ok = dialog.add_child(make("ok", {{10, 10}, {60, 30}}));
  behind.handles_all = true;
  ok.set_focus_policy(focus_policy::strong);

  REQUIRE(f.router.dispatch(press({10, 10})));
  REQUIRE(f.router.pointer_capture() == &behind);
  REQUIRE(f.tree.focus().push_modal(dialog));
  REQUIRE(ok.has_focus());

  // The capture outside the modal is cancelled on the next pointer event.
  REQUIRE_FALSE(f.router.dispatch(move({20, 20}, pointer_button::primary)));
  REQUIRE(behind.count(pointer_action::cancel) == 1);
  REQUIRE(f.router.pointer_capture() == nullptr);

  REQUIRE(f.router.hit_test({10, 10}) == nullptr);
  REQUIRE_FALSE(f.router.dispatch(press({10, 10})));
  REQUIRE(behind.count(pointer_action::down) == 1);
  REQUIRE(f.router.hit_test({170, 70}) == &ok);

  f.root->received.clear();
  REQUIRE_FALSE(f.router.dispatch(key_press(key::escape)));
  REQUIRE(dialog.received.size() == 1);
  REQUIRE(f.root->received.empty());

  REQUIRE(f.tree.focus().pop_modal(dialog));
  REQUIRE(f.router.hit_test({10, 10}) == &behind);
}

TEST_CASE("window events update the tree", "[event_router]") {
  fixture f;
  REQUIRE(
      f.router.dispatch(window_event{.action = window_action::resized, .viewport = {640, 480}}));
  REQUIRE(f.tree.viewport() == basalt::size{640, 480});
  REQUIRE(f.root->bounds().extent == basalt::size{640, 480});

  REQUIRE(
      f.router.dispatch(window_event{.action = window_action::scale_changed, .display_scale = 2}));
  REQUIRE(f.tree.scale().display == 2);
  REQUIRE_FALSE(
      f.router.dispatch(window_event{.action = window_action::scale_changed, .display_scale = 0}));
  REQUIRE(f.tree.scale().display == 2);

  REQUIRE(f.router.dispatch(window_event{.action = window_action::focus_lost}));
  REQUIRE_FALSE(f.tree.focus().window_active());
  REQUIRE(f.router.dispatch(window_event{.action = window_action::focus_gained}));
  REQUIRE(f.tree.focus().window_active());

  REQUIRE_FALSE(f.router.dispatch(window_event{.action = window_action::close_requested}));
}
