#pragma once

#include <cstdint>

namespace basalt {

struct rgba {
  std::uint8_t r = 0;
  std::uint8_t g = 0;
  std::uint8_t b = 0;
  std::uint8_t a = 255;

  [[nodiscard]] friend constexpr auto operator==(rgba const &, rgba const &) -> bool = default;
};

// WCAG 2.x relative luminance of the color, ignoring alpha.
[[nodiscard]] auto relative_luminance(rgba color) -> double;

// WCAG 2.x contrast ratio in [1, 21]; order of arguments does not matter.
[[nodiscard]] auto contrast_ratio(rgba a, rgba b) -> double;

// Composites `top` over an opaque `bottom` using straight alpha.
[[nodiscard]] auto blend_over(rgba top, rgba bottom) -> rgba;

// Multiplies the alpha channel by `opacity`, clamped to [0, 1].
[[nodiscard]] auto with_opacity(rgba color, float opacity) -> rgba;

// Converts a Cubit opacity token (a percentage) to a [0, 1] factor.
[[nodiscard]] constexpr auto opacity_from_percent(float percent) -> float {
  return percent < 0 ? 0.0f : percent > 100 ? 1.0f : percent / 100.0f;
}

} // namespace basalt
