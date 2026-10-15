#include "basalt/widget_painter.h"

#include "basalt/focus.h"
#include "basalt/render_context.h"

#include <cstdint>
#include <optional>

namespace basalt {

namespace {

// `area` is in the coordinates of `w`'s parent.
auto paint_widget(widget const &w, render_context &context, rect area, std::uint8_t dim,
                  bool ancestor_disabled) -> void {
  if (!w.visible() || !intersects(w.bounds(), area)) {
    return;
  }
  bool const disabled = ancestor_disabled || !w.enabled();
  std::optional<render_context::scope> dimmed;
  if (disabled && !ancestor_disabled) {
    dimmed.emplace(context.layer(dim));
  }
  point const origin = w.bounds().origin;
  auto const move = context.transform(affine_transform::translation(origin.x, origin.y));
  auto const clip = context.clip({{}, w.bounds().extent});
  rect const local_area = translated(area, {-origin.x, -origin.y});
  w.paint(context);
  for (widget const &child : w.children()) {
    paint_widget(child, context, local_area, dim, disabled);
  }
  w.paint_overlay(context);
}

} // namespace

auto paint_tree(widget_tree const &tree, paint_commands &commands, rect area) -> void {
  widget const *const root = tree.root();
  if (root == nullptr) {
    return;
  }
  render_context context(commands);
  auto const dim = static_cast<std::uint8_t>(tree.theme().disabled_opacity * 255.0f + 0.5f);
  paint_widget(*root, context, area, dim, false);
  if (auto const ring = visible_focus_ring(tree)) {
    auto const clip = context.clip(ring->clip);
    float const half = ring->width / 2;
    context.stroke_rounded_rect(
        {ring->outer.origin + point{half, half},
         {ring->outer.width() - ring->width, ring->outer.height() - ring->width}},
        ring->corner_radius > 0 ? ring->corner_radius - half : 0, ring->color, ring->width);
  }
}

auto paint_tree(widget_tree const &tree, paint_commands &commands) -> void {
  paint_tree(tree, commands, {{}, tree.viewport()});
}

} // namespace basalt
