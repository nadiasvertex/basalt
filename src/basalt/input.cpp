#include "basalt/input.h"

#include "basalt/event.h"

#include <array>

namespace basalt {
namespace {

constexpr std::array<std::string_view, static_cast<std::size_t>(key::menu) + 1> key_names = {
    "unknown",
    "a",
    "b",
    "c",
    "d",
    "e",
    "f",
    "g",
    "h",
    "i",
    "j",
    "k",
    "l",
    "m",
    "n",
    "o",
    "p",
    "q",
    "r",
    "s",
    "t",
    "u",
    "v",
    "w",
    "x",
    "y",
    "z",
    "0",
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "7",
    "8",
    "9",
    "f1",
    "f2",
    "f3",
    "f4",
    "f5",
    "f6",
    "f7",
    "f8",
    "f9",
    "f10",
    "f11",
    "f12",
    "left",
    "right",
    "up",
    "down",
    "home",
    "end",
    "page_up",
    "page_down",
    "enter",
    "escape",
    "tab",
    "space",
    "backspace",
    "delete",
    "insert",
    "minus",
    "equal",
    "left_bracket",
    "right_bracket",
    "backslash",
    "semicolon",
    "quote",
    "backquote",
    "comma",
    "period",
    "slash",
    "left_shift",
    "right_shift",
    "left_control",
    "right_control",
    "left_alt",
    "right_alt",
    "left_meta",
    "right_meta",
    "caps_lock",
    "menu",
};

} // namespace

auto key_name(key k) -> std::string_view {
  auto const index = static_cast<std::size_t>(k);
  return index < key_names.size() ? key_names[index] : "unknown";
}

auto modifier_of(key k) -> modifier {
  switch (k) {
  case key::left_shift:
  case key::right_shift:
    return modifier::shift;
  case key::left_control:
  case key::right_control:
    return modifier::control;
  case key::left_alt:
  case key::right_alt:
    return modifier::alt;
  case key::left_meta:
  case key::right_meta:
    return modifier::meta;
  default:
    return modifier::none;
  }
}

auto is_modifier_key(key k) -> bool {
  return k == key::caps_lock || modifier_of(k) != modifier::none;
}

auto event_time_of(event const &e) -> event_time {
  return std::visit([](auto const &value) { return value.time; }, e);
}

auto event_position(event const &e) -> std::optional<point> {
  if (auto const *pointer = std::get_if<pointer_event>(&e)) {
    return pointer->position;
  }
  if (auto const *wheel = std::get_if<wheel_event>(&e)) {
    return wheel->position;
  }
  if (auto const *drop = std::get_if<file_drop_event>(&e)) {
    return drop->position;
  }
  return std::nullopt;
}

auto is_keyboard_event(event const &e) -> bool {
  return std::holds_alternative<key_event>(e) || std::holds_alternative<text_input_event>(e) ||
         std::holds_alternative<ime_event>(e);
}

} // namespace basalt
