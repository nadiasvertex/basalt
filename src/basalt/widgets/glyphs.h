#pragma once

#include "basalt/color.h"
#include "basalt/geometry.h"
#include "basalt/render_context.h"

namespace basalt::glyphs {

// Marks drawn with strokes so that controls share one shape for each symbol.
// Each takes the square or box the mark should fill.

namespace detail {

[[nodiscard]] inline auto at(rect area, float x, float y) -> point {
  return {area.origin.x + area.width() * x, area.origin.y + area.height() * y};
}

} // namespace detail

inline auto draw_check(render_context &context, rect area, rgba color, float width) -> void {
  context.line(detail::at(area, 0.22f, 0.54f), detail::at(area, 0.42f, 0.74f), color, width);
  context.line(detail::at(area, 0.42f, 0.74f), detail::at(area, 0.80f, 0.28f), color, width);
}

inline auto draw_dash(render_context &context, rect area, rgba color, float width) -> void {
  context.line(detail::at(area, 0.25f, 0.5f), detail::at(area, 0.75f, 0.5f), color, width);
}

inline auto draw_chevron(render_context &context, rect area, bool pointing_down, rgba color,
                         float width) -> void {
  float const tip = pointing_down ? 0.65f : 0.35f;
  float const ends = pointing_down ? 0.35f : 0.65f;
  context.line(detail::at(area, 0.25f, ends), detail::at(area, 0.5f, tip), color, width);
  context.line(detail::at(area, 0.5f, tip), detail::at(area, 0.75f, ends), color, width);
}

// A tree row's mark: it points to the end of the line when collapsed and down when expanded.
inline auto draw_disclosure(render_context &context, rect area, bool expanded, rgba color,
                            float width) -> void {
  if (expanded) {
    draw_chevron(context, area, true, color, width);
    return;
  }
  context.line(detail::at(area, 0.35f, 0.25f), detail::at(area, 0.65f, 0.5f), color, width);
  context.line(detail::at(area, 0.65f, 0.5f), detail::at(area, 0.35f, 0.75f), color, width);
}

inline auto draw_close(render_context &context, rect area, rgba color, float width) -> void {
  context.line(detail::at(area, 0.3f, 0.3f), detail::at(area, 0.7f, 0.7f), color, width);
  context.line(detail::at(area, 0.7f, 0.3f), detail::at(area, 0.3f, 0.7f), color, width);
}

} // namespace basalt::glyphs
