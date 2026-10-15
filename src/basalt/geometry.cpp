#include "basalt/geometry.h"

#include <array>
#include <cmath>

namespace basalt {

auto affine_transform::rotation(float radians) -> affine_transform {
  // Quarter turns must be exact so they still preserve axis alignment.
  auto snap = [](float v) { return std::abs(v) < 1e-6f ? 0.0f : v; };
  float const c = snap(std::cos(radians));
  float const s = snap(std::sin(radians));
  return {c, s, -s, c, 0, 0};
}

auto transform_bounds(affine_transform const &t, rect r) -> rect {
  std::array const corners{
      apply(t, {r.left(), r.top()}),
      apply(t, {r.right(), r.top()}),
      apply(t, {r.left(), r.bottom()}),
      apply(t, {r.right(), r.bottom()}),
  };
  auto const [min_x, max_x] = std::ranges::minmax(corners, {}, &point::x);
  auto const [min_y, max_y] = std::ranges::minmax(corners, {}, &point::y);
  return rect::from_edges(min_x.x, min_y.y, max_x.x, max_y.y);
}

auto invert(affine_transform const &t) -> std::optional<affine_transform> {
  float const det = t.xx * t.yy - t.xy * t.yx;
  if (det == 0 || !std::isfinite(det)) {
    return std::nullopt;
  }
  float const inv = 1.0f / det;
  float const xx = t.yy * inv;
  float const yx = -t.yx * inv;
  float const xy = -t.xy * inv;
  float const yy = t.xx * inv;
  return affine_transform{xx, yx, xy, yy, -(xx * t.tx + xy * t.ty), -(yx * t.tx + yy * t.ty)};
}

} // namespace basalt
