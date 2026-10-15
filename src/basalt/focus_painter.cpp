#include "basalt/focus.h"
#include "basalt/widget_tree.h"

#include <optional>

namespace basalt {

auto make_focus_ring(widget const &w, focus_ring_shape const &shape) -> focus_ring {
  widget_tree const *const tree = w.tree();
  basalt::theme const &theme = w.theme();
  scale_factors const scale = w.scale();
  float const width = device_stroke_width(theme.sizes.dimension_focus_ring, scale);
  float const gap = w.resolve(theme.sizes.gap_focus_ring);
  rect const area = translated(shape.area, w.to_root({}));
  rect const outer = snap_to_device(outset(area, insets::uniform(gap + width)), scale);

  rect clip = tree != nullptr ? rect{{}, tree->viewport()} : outer;
  for (widget const *p = w.parent(); p != nullptr; p = p->parent()) {
    clip = intersection(clip, p->bounds_in_root());
  }
  return {
      .outer = outer,
      .width = width,
      .corner_radius = shape.corner_radius > 0 ? shape.corner_radius + gap + width : 0,
      .color = theme.colors.border_focus,
      .clip = clip,
  };
}

auto visible_focus_ring(widget_tree const &tree) -> std::optional<focus_ring> {
  widget const *const focused = tree.focus().focused();
  if (focused == nullptr || !focused->interaction().focus_visible || !focused->visible_in_tree()) {
    return std::nullopt;
  }
  auto const shape = focused->focus_ring();
  if (!shape) {
    return std::nullopt;
  }
  return make_focus_ring(*focused, *shape);
}

} // namespace basalt
