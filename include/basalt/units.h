#pragma once

#include "basalt/geometry.h"

#include <cstdint>

namespace basalt {

// Three pixel spaces are involved in sizing:
//
// - Token pixels: the values in Cubit tokens and widget code.
// - Layout pixels: token pixels after the application zoom and, for relative
//   lengths, the user's text scale. Layout, hit testing and paint commands use
//   this space; it matches the platform's logical window coordinates.
// - Device pixels: layout pixels times the display's content scale.
//
// Fixed lengths follow zoom only, so borders and fixed indicators keep their
// proportions. Relative lengths (text and anything sized to it) also follow
// the text scale, which is how 200% text size preferences are honoured.
enum class unit : std::uint8_t { fixed, relative };

struct length {
  float value = 0;
  basalt::unit kind = unit::fixed;

  [[nodiscard]] friend constexpr auto operator==(length const &, length const &) -> bool = default;
};

[[nodiscard]] constexpr auto fixed(float token_pixels) -> length {
  return {token_pixels, unit::fixed};
}
[[nodiscard]] constexpr auto relative(float token_pixels) -> length {
  return {token_pixels, unit::relative};
}

[[nodiscard]] constexpr auto operator*(length l, float factor) -> length {
  return {l.value * factor, l.kind};
}

struct scale_factors {
  // Display content scale reported by the platform (2 on most HiDPI screens).
  float display = 1;
  // Application zoom preference; scales every length.
  float zoom = 1;
  // User text-size preference; scales relative lengths only.
  float text = 1;

  [[nodiscard]] friend constexpr auto operator==(scale_factors const &, scale_factors const &)
      -> bool = default;
};

// True when every factor is finite and positive.
[[nodiscard]] auto is_valid(scale_factors factors) -> bool;

[[nodiscard]] constexpr auto to_layout(length l, scale_factors factors) -> float {
  float const scale = l.kind == unit::relative ? factors.zoom * factors.text : factors.zoom;
  return l.value * scale;
}

// The inverse of `to_layout`: a length of kind `kind` that resolves to
// `layout_pixels` under `factors`.
[[nodiscard]] constexpr auto from_layout(float layout_pixels, unit kind, scale_factors factors)
    -> length {
  float const scale = kind == unit::relative ? factors.zoom * factors.text : factors.zoom;
  return {layout_pixels / scale, kind};
}

[[nodiscard]] constexpr auto to_device(float layout_pixels, scale_factors factors) -> float {
  return layout_pixels * factors.display;
}
[[nodiscard]] constexpr auto to_device(point p, scale_factors factors) -> point {
  return {to_device(p.x, factors), to_device(p.y, factors)};
}
[[nodiscard]] constexpr auto to_device(rect r, scale_factors factors) -> rect {
  return {to_device(r.origin, factors),
          {to_device(r.extent.width, factors), to_device(r.extent.height, factors)}};
}
[[nodiscard]] constexpr auto from_device(float device_pixels, scale_factors factors) -> float {
  return device_pixels / factors.display;
}
[[nodiscard]] constexpr auto from_device(point p, scale_factors factors) -> point {
  return {from_device(p.x, factors), from_device(p.y, factors)};
}

// Maps layout coordinates to device coordinates, for the renderer's root transform.
[[nodiscard]] constexpr auto device_transform(scale_factors factors) -> affine_transform {
  return affine_transform::scaling(factors.display, factors.display);
}

// Rounds a layout coordinate to the nearest device pixel boundary so edges stay crisp.
[[nodiscard]] auto snap_to_device(float layout_pixels, scale_factors factors) -> float;

// Snaps each edge independently, so adjacent snapped rects still abut.
[[nodiscard]] auto snap_to_device(rect r, scale_factors factors) -> rect;

// A hairline that is at least one device pixel wide, in layout pixels.
[[nodiscard]] auto device_stroke_width(length l, scale_factors factors) -> float;

} // namespace basalt
