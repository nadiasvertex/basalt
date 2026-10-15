#pragma once

#include "basalt/geometry.h"

#include <cstdint>
#include <type_traits>

namespace basalt {

// Interaction flags kept on every widget by the event router and the focus
// manager. Widgets read them through `widget::interaction()` and are told of
// changes through `widget::on_interaction_changed`.
struct interaction_state {
  // The pointer is over this widget or one of its descendants.
  bool hovered = false;
  // A primary press started on this widget or a descendant and the pointer is
  // still over the widget it started on.
  bool pressed = false;
  // This widget has keyboard focus.
  bool focused = false;
  // This widget or a descendant has keyboard focus.
  bool focus_within = false;
  // This widget has focus that should be shown with a focus ring: the user is
  // navigating with the keyboard and the window is active.
  bool focus_visible = false;

  [[nodiscard]] friend constexpr auto operator==(interaction_state const &,
                                                 interaction_state const &) -> bool = default;
};

// The mutually exclusive Cubit appearance states of a control. Focus and
// selection are shown on top of these.
enum class control_state : std::uint8_t { normal, hovered, pressed, disabled };

// Disabled wins over pressed, which wins over hovered.
[[nodiscard]] constexpr auto control_state_of(interaction_state const &state, bool enabled)
    -> control_state {
  if (!enabled) {
    return control_state::disabled;
  }
  if (state.pressed) {
    return control_state::pressed;
  }
  return state.hovered ? control_state::hovered : control_state::normal;
}

// Picks the value for `state`. A disabled control shows its normal value.
template <class T>
[[nodiscard]] constexpr auto by_state(control_state state, T normal,
                                      std::type_identity_t<T> hovered,
                                      std::type_identity_t<T> pressed) -> T {
  switch (state) {
  case control_state::hovered:
    return hovered;
  case control_state::pressed:
    return pressed;
  case control_state::normal:
  case control_state::disabled:
    break;
  }
  return normal;
}

// How a widget may receive keyboard focus. Every policy except `none` accepts
// programmatic focus.
enum class focus_policy : std::uint8_t {
  none,
  // Focused by a pointer press; skipped by Tab.
  click,
  // A Tab stop; a pointer press does not focus it.
  tab,
  // Both.
  strong,
};

[[nodiscard]] constexpr auto focus_by_click(focus_policy p) -> bool {
  return p == focus_policy::click || p == focus_policy::strong;
}
[[nodiscard]] constexpr auto focus_by_tab(focus_policy p) -> bool {
  return p == focus_policy::tab || p == focus_policy::strong;
}

// How a widget groups the focus traversal of its descendants.
enum class focus_scope : std::uint8_t {
  none,
  // The descendants form one Tab stop (a toolbar, a tab list, a radio group).
  // Tab enters at the descendant focused last, or the first one, and the
  // widget moves among them with `focus_manager::move_focus_in_group`.
  group,
};

enum class focus_reason : std::uint8_t { keyboard, pointer, programmatic };

// Where a widget's focus ring goes, in the widget's coordinates. The ring is
// drawn outside `area`, separated from it by the Cubit focus-ring gap.
struct focus_ring_shape {
  rect area;
  float corner_radius = 0;

  [[nodiscard]] friend constexpr auto operator==(focus_ring_shape const &, focus_ring_shape const &)
      -> bool = default;
};

} // namespace basalt
