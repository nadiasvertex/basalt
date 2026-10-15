#pragma once

#include <cstdint>
#include <string_view>

namespace basalt {

// Platform-neutral input vocabulary. Backends translate native codes into these.

enum class key : std::uint16_t {
  unknown,
  a,
  b,
  c,
  d,
  e,
  f,
  g,
  h,
  i,
  j,
  k,
  l,
  m,
  n,
  o,
  p,
  q,
  r,
  s,
  t,
  u,
  v,
  w,
  x,
  y,
  z,
  digit_0,
  digit_1,
  digit_2,
  digit_3,
  digit_4,
  digit_5,
  digit_6,
  digit_7,
  digit_8,
  digit_9,
  f1,
  f2,
  f3,
  f4,
  f5,
  f6,
  f7,
  f8,
  f9,
  f10,
  f11,
  f12,
  left,
  right,
  up,
  down,
  home,
  end,
  page_up,
  page_down,
  enter,
  escape,
  tab,
  space,
  backspace,
  delete_forward,
  insert,
  minus,
  equal,
  left_bracket,
  right_bracket,
  backslash,
  semicolon,
  quote,
  backquote,
  comma,
  period,
  slash,
  left_shift,
  right_shift,
  left_control,
  right_control,
  left_alt,
  right_alt,
  left_meta,
  right_meta,
  caps_lock,
  menu,
};

[[nodiscard]] auto key_name(key k) -> std::string_view;
[[nodiscard]] auto is_modifier_key(key k) -> bool;

enum class modifier : std::uint8_t {
  none = 0,
  shift = 1 << 0,
  control = 1 << 1,
  alt = 1 << 2,
  // Command on macOS, Windows/Super key elsewhere.
  meta = 1 << 3,
};

[[nodiscard]] constexpr auto operator|(modifier a, modifier b) -> modifier {
  return static_cast<modifier>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}
[[nodiscard]] constexpr auto operator&(modifier a, modifier b) -> modifier {
  return static_cast<modifier>(static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b));
}
constexpr auto operator|=(modifier &a, modifier b) -> modifier & { return a = a | b; }

[[nodiscard]] constexpr auto has(modifier set, modifier flag) -> bool {
  return flag != modifier::none && (set & flag) == flag;
}

// Modifier set contributed by pressing or releasing `k`; none for other keys.
[[nodiscard]] auto modifier_of(key k) -> modifier;

enum class pointer_button : std::uint8_t {
  none = 0,
  primary = 1 << 0,
  secondary = 1 << 1,
  middle = 1 << 2,
  back = 1 << 3,
  forward = 1 << 4,
};

[[nodiscard]] constexpr auto operator|(pointer_button a, pointer_button b) -> pointer_button {
  return static_cast<pointer_button>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}
[[nodiscard]] constexpr auto operator&(pointer_button a, pointer_button b) -> pointer_button {
  return static_cast<pointer_button>(static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b));
}
constexpr auto operator|=(pointer_button &a, pointer_button b) -> pointer_button & {
  return a = a | b;
}

[[nodiscard]] constexpr auto has(pointer_button set, pointer_button flag) -> bool {
  return flag != pointer_button::none && (set & flag) == flag;
}

enum class pointer_kind : std::uint8_t { mouse, touch, pen };

} // namespace basalt
