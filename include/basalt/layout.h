#pragma once

#include "basalt/geometry.h"
#include "basalt/units.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace basalt {

// Layout math shared by the container widgets. Everything here is in layout
// pixels; resolve token lengths first with `to_layout` or `widget::resolve`.

inline constexpr float unbounded = std::numeric_limits<float>::infinity();

enum class axis : std::uint8_t { horizontal, vertical };

[[nodiscard]] constexpr auto cross_axis(axis a) -> axis {
  return a == axis::horizontal ? axis::vertical : axis::horizontal;
}

[[nodiscard]] constexpr auto main_extent(basalt::size s, axis a) -> float {
  return a == axis::horizontal ? s.width : s.height;
}
[[nodiscard]] constexpr auto cross_extent(basalt::size s, axis a) -> float {
  return a == axis::horizontal ? s.height : s.width;
}
[[nodiscard]] constexpr auto make_size(axis a, float main, float cross) -> basalt::size {
  return a == axis::horizontal ? basalt::size{main, cross} : basalt::size{cross, main};
}
[[nodiscard]] constexpr auto make_point(axis a, float main, float cross) -> point {
  return a == axis::horizontal ? point{main, cross} : point{cross, main};
}
[[nodiscard]] constexpr auto main_coordinate(point p, axis a) -> float {
  return a == axis::horizontal ? p.x : p.y;
}

// Where a child sits in the free space along one axis. `stretch` fills the
// space; the other values keep the child's measured size.
enum class alignment : std::uint8_t { start, center, end, stretch };

// Offset of something `free` pixels smaller than its slot.
[[nodiscard]] constexpr auto align_offset(alignment a, float free) -> float {
  switch (a) {
  case alignment::center:
    return free / 2;
  case alignment::end:
    return free;
  case alignment::start:
  case alignment::stretch:
    break;
  }
  return 0;
}

// The sizes a widget may take. `max` may be `unbounded` on either axis, which
// asks the widget for its natural extent (used by scrolling axes).
struct constraints {
  basalt::size min;
  basalt::size max{unbounded, unbounded};

  [[nodiscard]] static constexpr auto tight(basalt::size s) -> constraints { return {s, s}; }
  [[nodiscard]] static constexpr auto loose(basalt::size s) -> constraints { return {{}, s}; }

  [[nodiscard]] constexpr auto bounded_width() const -> bool { return max.width < unbounded; }
  [[nodiscard]] constexpr auto bounded_height() const -> bool { return max.height < unbounded; }

  // Clamps `s` into the limits; `min` wins when the limits conflict.
  [[nodiscard]] constexpr auto constrain(basalt::size s) const -> basalt::size {
    return {std::max(min.width, std::min(s.width, max.width)),
            std::max(min.height, std::min(s.height, max.height))};
  }

  // The limits left for content inside `edges`; never negative.
  [[nodiscard]] constexpr auto deflate(insets edges) const -> constraints {
    auto const shrink = [](float v, float by) { return std::max(0.0f, v - by); };
    return {{shrink(min.width, edges.horizontal()), shrink(min.height, edges.vertical())},
            {shrink(max.width, edges.horizontal()), shrink(max.height, edges.vertical())}};
  }

  [[nodiscard]] friend constexpr auto operator==(constraints const &, constraints const &)
      -> bool = default;
};

[[nodiscard]] constexpr auto inflate(basalt::size s, insets edges) -> basalt::size {
  return {s.width + edges.horizontal(), s.height + edges.vertical()};
}

// Insets whose edges are token lengths, so padding can follow zoom only
// (fixed) or zoom and text scale (relative).
struct length_insets {
  length top;
  length right;
  length bottom;
  length left;

  [[nodiscard]] static constexpr auto uniform(length all) -> length_insets {
    return {all, all, all, all};
  }
  [[nodiscard]] static constexpr auto symmetric(length horizontal, length vertical)
      -> length_insets {
    return {vertical, horizontal, vertical, horizontal};
  }

  [[nodiscard]] friend constexpr auto operator==(length_insets const &, length_insets const &)
      -> bool = default;
};

[[nodiscard]] constexpr auto to_layout(length_insets const &edges, scale_factors factors)
    -> insets {
  return {to_layout(edges.top, factors), to_layout(edges.right, factors),
          to_layout(edges.bottom, factors), to_layout(edges.left, factors)};
}

// One child along the main axis of a linear layout.
struct linear_item {
  // Size before growing or shrinking.
  float basis = 0;
  float min = 0;
  float max = unbounded;
  // Share of positive free space.
  float grow = 0;
  // Share of a shortfall, weighted by `basis`.
  float shrink = 0;
};

// Sizes `items` to fill `available` the way CSS flexbox does: free space is
// shared by `grow`, a shortfall by `shrink * basis`, and items that hit their
// `min` or `max` are frozen while the rest is redistributed. The result can
// still overflow or underfill when the limits or factors do not allow
// otherwise.
[[nodiscard]] auto distribute(std::span<linear_item const> items, float available)
    -> std::vector<float>;

} // namespace basalt
