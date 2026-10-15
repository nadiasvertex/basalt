#pragma once

#include <algorithm>
#include <optional>

namespace basalt {

// Coordinates are layout pixels (see basalt/units.h) with y growing downward.

struct point {
  float x = 0;
  float y = 0;

  [[nodiscard]] friend constexpr auto operator==(point const &, point const &) -> bool = default;
};

[[nodiscard]] constexpr auto operator+(point a, point b) -> point { return {a.x + b.x, a.y + b.y}; }
[[nodiscard]] constexpr auto operator-(point a, point b) -> point { return {a.x - b.x, a.y - b.y}; }

struct size {
  float width = 0;
  float height = 0;

  [[nodiscard]] constexpr auto empty() const -> bool { return width <= 0 || height <= 0; }

  [[nodiscard]] friend constexpr auto operator==(size const &, size const &) -> bool = default;
};

struct insets {
  float top = 0;
  float right = 0;
  float bottom = 0;
  float left = 0;

  [[nodiscard]] static constexpr auto uniform(float all) -> insets { return {all, all, all, all}; }
  [[nodiscard]] static constexpr auto symmetric(float horizontal, float vertical) -> insets {
    return {vertical, horizontal, vertical, horizontal};
  }

  [[nodiscard]] constexpr auto horizontal() const -> float { return left + right; }
  [[nodiscard]] constexpr auto vertical() const -> float { return top + bottom; }

  [[nodiscard]] friend constexpr auto operator==(insets const &, insets const &) -> bool = default;
};

struct rect {
  point origin;
  basalt::size extent;

  [[nodiscard]] static constexpr auto from_edges(float left, float top, float right, float bottom)
      -> rect {
    return {{left, top}, {std::max(0.0f, right - left), std::max(0.0f, bottom - top)}};
  }

  [[nodiscard]] constexpr auto left() const -> float { return origin.x; }
  [[nodiscard]] constexpr auto top() const -> float { return origin.y; }
  [[nodiscard]] constexpr auto right() const -> float { return origin.x + extent.width; }
  [[nodiscard]] constexpr auto bottom() const -> float { return origin.y + extent.height; }
  [[nodiscard]] constexpr auto width() const -> float { return extent.width; }
  [[nodiscard]] constexpr auto height() const -> float { return extent.height; }
  [[nodiscard]] constexpr auto empty() const -> bool { return extent.empty(); }
  [[nodiscard]] constexpr auto center() const -> point {
    return {origin.x + extent.width / 2, origin.y + extent.height / 2};
  }

  // Half-open: the right and bottom edges belong to the neighbouring rect.
  [[nodiscard]] constexpr auto contains(point p) const -> bool {
    return p.x >= left() && p.x < right() && p.y >= top() && p.y < bottom();
  }

  [[nodiscard]] friend constexpr auto operator==(rect const &, rect const &) -> bool = default;
};

[[nodiscard]] constexpr auto translated(rect r, point delta) -> rect {
  return {r.origin + delta, r.extent};
}

// Shrinks `r` by `edges`; the result never has a negative extent.
[[nodiscard]] constexpr auto inset(rect r, insets edges) -> rect {
  return rect::from_edges(r.left() + edges.left, r.top() + edges.top, r.right() - edges.right,
                          r.bottom() - edges.bottom);
}

[[nodiscard]] constexpr auto outset(rect r, insets edges) -> rect {
  return rect::from_edges(r.left() - edges.left, r.top() - edges.top, r.right() + edges.right,
                          r.bottom() + edges.bottom);
}

[[nodiscard]] constexpr auto intersects(rect a, rect b) -> bool {
  return !a.empty() && !b.empty() && a.left() < b.right() && b.left() < a.right() &&
         a.top() < b.bottom() && b.top() < a.bottom();
}

// Empty when the rects do not overlap.
[[nodiscard]] constexpr auto intersection(rect a, rect b) -> rect {
  if (!intersects(a, b)) {
    return {};
  }
  return rect::from_edges(std::max(a.left(), b.left()), std::max(a.top(), b.top()),
                          std::min(a.right(), b.right()), std::min(a.bottom(), b.bottom()));
}

// Smallest rect containing both; empty rects are ignored.
[[nodiscard]] constexpr auto bounding_union(rect a, rect b) -> rect {
  if (a.empty()) {
    return b;
  }
  if (b.empty()) {
    return a;
  }
  return rect::from_edges(std::min(a.left(), b.left()), std::min(a.top(), b.top()),
                          std::max(a.right(), b.right()), std::max(a.bottom(), b.bottom()));
}

// 2D affine transform mapping (x, y) to (xx*x + xy*y + tx, yx*x + yy*y + ty).
struct affine_transform {
  float xx = 1;
  float yx = 0;
  float xy = 0;
  float yy = 1;
  float tx = 0;
  float ty = 0;

  [[nodiscard]] static constexpr auto identity() -> affine_transform { return {}; }
  [[nodiscard]] static constexpr auto translation(float dx, float dy) -> affine_transform {
    return {1, 0, 0, 1, dx, dy};
  }
  [[nodiscard]] static constexpr auto scaling(float sx, float sy) -> affine_transform {
    return {sx, 0, 0, sy, 0, 0};
  }
  // Positive angles rotate clockwise on screen because y grows downward.
  [[nodiscard]] static auto rotation(float radians) -> affine_transform;

  [[nodiscard]] constexpr auto is_identity() const -> bool { return *this == affine_transform{}; }
  // True when axis-aligned rects stay axis-aligned (no rotation or skew).
  [[nodiscard]] constexpr auto preserves_axis_alignment() const -> bool {
    return (xy == 0 && yx == 0) || (xx == 0 && yy == 0);
  }

  [[nodiscard]] friend constexpr auto operator==(affine_transform const &, affine_transform const &)
      -> bool = default;
};

// `outer * inner` applies `inner` first, then `outer`.
[[nodiscard]] constexpr auto operator*(affine_transform const &outer, affine_transform const &inner)
    -> affine_transform {
  return {
      outer.xx * inner.xx + outer.xy * inner.yx,
      outer.yx * inner.xx + outer.yy * inner.yx,
      outer.xx * inner.xy + outer.xy * inner.yy,
      outer.yx * inner.xy + outer.yy * inner.yy,
      outer.xx * inner.tx + outer.xy * inner.ty + outer.tx,
      outer.yx * inner.tx + outer.yy * inner.ty + outer.ty,
  };
}

[[nodiscard]] constexpr auto apply(affine_transform const &t, point p) -> point {
  return {t.xx * p.x + t.xy * p.y + t.tx, t.yx * p.x + t.yy * p.y + t.ty};
}

// Axis-aligned bounds of the transformed rect.
[[nodiscard]] auto transform_bounds(affine_transform const &t, rect r) -> rect;

// Empty when the transform is singular.
[[nodiscard]] auto invert(affine_transform const &t) -> std::optional<affine_transform>;

} // namespace basalt
