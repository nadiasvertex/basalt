#include "basalt/widgets/separator.h"

#include "basalt/render_context.h"
#include "basalt/widget_tree.h"

namespace basalt {

namespace {

// Space on each side of the line, so grouped controls do not touch it.
constexpr length margin = fixed(4);
constexpr length thickness = fixed(1);

} // namespace

auto separator::set_direction(axis direction) -> void {
  if (direction != direction_) {
    direction_ = direction;
    invalidate_layout();
    invalidate_paint();
  }
}

auto separator::accessibility_info() const -> accessibility::info {
  accessibility::info details;
  details.kind = accessibility::role::separator;
  details.orientation = direction_ == axis::horizontal ? accessibility::orientation::horizontal
                                                       : accessibility::orientation::vertical;
  return details;
}

auto separator::on_measure(constraints const &) const -> basalt::size {
  float const across = resolve(thickness) + 2 * resolve(margin);
  return make_size(direction_, 0, across);
}

auto separator::on_paint(render_context &context) const -> void {
  if (tree() == nullptr) {
    return;
  }
  scale_factors const factors = scale();
  float const width = device_stroke_width(thickness, factors);
  basalt::size const extent = bounds().extent;
  rect line;
  if (direction_ == axis::horizontal) {
    line = {{0, snap_to_device((extent.height - width) / 2, factors)}, {extent.width, width}};
  } else {
    line = {{snap_to_device((extent.width - width) / 2, factors), 0}, {width, extent.height}};
  }
  context.fill_rect(line, tree()->theme().colors.border_subdued);
}

} // namespace basalt
