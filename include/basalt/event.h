#pragma once

#include "basalt/geometry.h"
#include "basalt/input.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace basalt {

// Normalized events. Positions are layout pixels in window coordinates (see basalt/units.h);
// text is UTF-8; timestamps are monotonic and only meaningful relative to each other.

using event_time = std::chrono::nanoseconds;

enum class pointer_action : std::uint8_t {
  move,
  down,
  up,
  // The platform took the pointer away (touch cancelled, capture lost).
  cancel,
  enter,
  leave,
};

struct pointer_event {
  pointer_action action = pointer_action::move;
  point position;
  // Button that changed for down/up; none otherwise.
  pointer_button button = pointer_button::none;
  // Every button held after this event.
  pointer_button buttons = pointer_button::none;
  modifier modifiers = modifier::none;
  pointer_kind kind = pointer_kind::mouse;
  // Consecutive presses at about the same spot; 1 for a single click.
  std::uint8_t click_count = 1;
  event_time time{};
};

struct wheel_event {
  point position;
  // Positive x scrolls content left (reveals the right); positive y scrolls content up
  // (reveals the bottom). Layout pixels when `precise`, otherwise whole wheel notches.
  point delta;
  bool precise = false;
  modifier modifiers = modifier::none;
  event_time time{};
};

struct key_event {
  key code = key::unknown;
  bool pressed = true;
  bool repeat = false;
  // State after this event, including the key itself when it is a modifier.
  modifier modifiers = modifier::none;
  event_time time{};
};

// Committed text, whether typed, pasted by an input method or composed from dead keys.
struct text_input_event {
  std::string text;
  event_time time{};
};

enum class ime_phase : std::uint8_t {
  // Composition text replaced; an empty string clears the preedit.
  update,
  // The input method abandoned the composition. Committed text arrives as text_input_event.
  cancel,
};

struct ime_event {
  ime_phase phase = ime_phase::update;
  // Preedit text.
  std::string text;
  // UTF-8 byte range of `text` the input method has selected; the caret when length is 0.
  std::size_t selection_start = 0;
  std::size_t selection_length = 0;
  event_time time{};
};

enum class window_action : std::uint8_t {
  resized,
  scale_changed,
  focus_gained,
  focus_lost,
  close_requested,
  shown,
  hidden,
};

struct window_event {
  window_action action = window_action::resized;
  // Layout-pixel viewport; set for resized.
  basalt::size viewport;
  // Device pixels per layout pixel; set for scale_changed.
  float display_scale = 1;
  event_time time{};
};

// An application command invoked by menu, shortcut or accessibility action.
struct command_event {
  std::string id;
  event_time time{};
};

using event = std::variant<pointer_event, wheel_event, key_event, text_input_event, ime_event,
                           window_event, command_event>;

[[nodiscard]] auto event_time_of(event const &e) -> event_time;

// Pointer position for events that have one.
[[nodiscard]] auto event_position(event const &e) -> std::optional<point>;

// Events delivered to the focused widget rather than the widget under the pointer.
[[nodiscard]] auto is_keyboard_event(event const &e) -> bool;

} // namespace basalt
