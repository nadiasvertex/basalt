#include "basalt/color.h"

#include <algorithm>
#include <cmath>

namespace basalt {
namespace {

auto linearize(std::uint8_t channel) -> double {
  double const c = channel / 255.0;
  return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}

} // namespace

auto relative_luminance(rgba color) -> double {
  return 0.2126 * linearize(color.r) + 0.7152 * linearize(color.g) + 0.0722 * linearize(color.b);
}

auto contrast_ratio(rgba a, rgba b) -> double {
  double const la = relative_luminance(a);
  double const lb = relative_luminance(b);
  return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

auto blend_over(rgba top, rgba bottom) -> rgba {
  double const alpha = top.a / 255.0;
  auto mix = [&](std::uint8_t t, std::uint8_t b) {
    return static_cast<std::uint8_t>(std::lround(t * alpha + b * (1.0 - alpha)));
  };
  return {mix(top.r, bottom.r), mix(top.g, bottom.g), mix(top.b, bottom.b), 255};
}

auto with_opacity(rgba color, float opacity) -> rgba {
  float const factor = std::isnan(opacity) ? 0.0f : std::clamp(opacity, 0.0f, 1.0f);
  color.a = static_cast<std::uint8_t>(std::lround(color.a * factor));
  return color;
}

} // namespace basalt
