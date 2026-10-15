#pragma once

#include "basalt/event_router.h"
#include "basalt/widget.h"

#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace basalt::testing {

// A widget that records the events it receives and optionally handles them.
struct input_probe : widget {
  explicit input_probe(std::string name = {}, rect area = {}) : name(std::move(name)) {
    set_bounds(area);
  }

  std::string name;
  // Handles every event when set, unless `handler` decides.
  bool handles_all = false;
  std::function<bool(event_context &, event const &)> handler;
  std::vector<event> received;
  std::vector<interaction_state> transitions;
  std::optional<focus_ring_shape> ring = std::nullopt;
  bool custom_ring = false;

  [[nodiscard]] auto last_pointer() const -> pointer_event const & {
    for (auto it = received.rbegin(); it != received.rend(); ++it) {
      if (auto const *p = std::get_if<pointer_event>(&*it)) {
        return *p;
      }
    }
    throw std::logic_error("no pointer event received");
  }

  [[nodiscard]] auto count(pointer_action action) const -> int {
    int n = 0;
    for (auto const &e : received) {
      if (auto const *p = std::get_if<pointer_event>(&e); p != nullptr && p->action == action) {
        ++n;
      }
    }
    return n;
  }

  [[nodiscard]] auto focus_ring() const -> std::optional<focus_ring_shape> override {
    return custom_ring ? ring : widget::focus_ring();
  }

protected:
  auto on_event(event_context &context, event const &e) -> bool override {
    received.push_back(e);
    if (handler) {
      return handler(context, e);
    }
    return handles_all;
  }
  auto on_interaction_changed(interaction_state const &previous) -> void override {
    transitions.push_back(previous);
    widget::on_interaction_changed(previous);
  }
};

[[nodiscard]] inline auto press(point at, pointer_button button = pointer_button::primary)
    -> pointer_event {
  return {.action = pointer_action::down, .position = at, .button = button, .buttons = button};
}
[[nodiscard]] inline auto release(point at, pointer_button button = pointer_button::primary)
    -> pointer_event {
  return {.action = pointer_action::up, .position = at, .button = button};
}
[[nodiscard]] inline auto move(point at, pointer_button held = pointer_button::none)
    -> pointer_event {
  return {.action = pointer_action::move, .position = at, .buttons = held};
}
[[nodiscard]] inline auto key_press(key code, modifier modifiers = modifier::none) -> key_event {
  return {.code = code, .pressed = true, .modifiers = modifiers};
}

} // namespace basalt::testing
