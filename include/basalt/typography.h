#pragma once

#include "basalt/units.h"

#include <cstdint>
#include <string_view>

namespace basalt {

enum class font_weight : std::uint16_t { regular = 400, semibold = 600, bold = 700 };

// Source and other fixed-pitch text; Cubit tokens only define the UI family.
inline constexpr std::string_view monospace_family = "Noto Sans Mono";

// A Cubit text style. Metrics are token pixels in relative units, so they
// follow both zoom and the text scale. `family` must refer to storage with
// static lifetime, such as a token or a string literal.
struct text_style {
  std::string_view family;
  float font_size = 0;
  font_weight weight = font_weight::regular;
  float letter_spacing = 0;
  float line_height = 0;

  [[nodiscard]] friend constexpr auto operator==(text_style const &, text_style const &)
      -> bool = default;
};

// Metrics converted to layout pixels.
[[nodiscard]] constexpr auto to_layout(text_style style, scale_factors factors) -> text_style {
  auto scale = [&](float v) { return basalt::to_layout(relative(v), factors); };
  return {style.family, scale(style.font_size), style.weight, scale(style.letter_spacing),
          scale(style.line_height)};
}

[[nodiscard]] constexpr auto with_family(text_style style, std::string_view family) -> text_style {
  style.family = family;
  return style;
}

[[nodiscard]] constexpr auto with_line_height(text_style style, float line_height) -> text_style {
  style.line_height = line_height;
  return style;
}

} // namespace basalt
