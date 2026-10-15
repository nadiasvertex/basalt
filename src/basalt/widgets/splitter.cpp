#include "basalt/widgets/splitter.h"

#include "basalt/event_router.h"
#include "basalt/render_context.h"
#include "basalt/widget_tree.h"
#include "basalt/widgets/split_view.h"

#include <algorithm>
#include <limits>
#include <variant>

namespace basalt {

splitter::splitter(split_view &owner, axis direction) : owner_(&owner), direction_(direction) {
  set_focus_policy(focus_policy::strong);
}

auto splitter::accessibility_info() const -> accessibility::info {
  accessibility::info details;
  details.kind = accessibility::role::splitter;
  details.name = "Resize panes";
  // A split with side-by-side panes has a vertical divider.
  details.orientation = direction_ == axis::horizontal ? accessibility::orientation::vertical
                                                       : accessibility::orientation::horizontal;
  float const position = main_coordinate(owner_->divider_rect().origin, direction_);
  float const space = main_extent(owner_->bounds().extent, direction_);
  details.range = accessibility::value_range{0, space, std::max(1.0f, resolve(key_step))};
  details.numeric_value = position;
  details.actions = {accessibility::action::increment, accessibility::action::decrement,
                     accessibility::action::focus};
  return details;
}

auto splitter::on_accessibility_action(accessibility::action_request const &request) -> bool {
  float const step = resolve(key_step);
  switch (request.kind) {
  case accessibility::action::increment:
    owner_->move_divider_by(step);
    return true;
  case accessibility::action::decrement:
    owner_->move_divider_by(-step);
    return true;
  default:
    return false;
  }
}

auto splitter::focus_ring() const -> std::optional<focus_ring_shape> {
  return focus_ring_shape{{{}, bounds().extent}, 0};
}

auto splitter::on_event(event_context &context, event const &e) -> bool {
  if (auto const *p = std::get_if<pointer_event>(&e)) {
    // The pointer is in this widget's coordinates; the divider's coordinates
    // are the owner's, which this widget's origin offsets.
    float const along =
        main_coordinate(p->position, direction_) + main_coordinate(bounds().origin, direction_);
    float const leading = main_coordinate(owner_->divider_rect().origin, direction_);
    switch (p->action) {
    case pointer_action::down:
      if (p->button != pointer_button::primary) {
        return false;
      }
      dragging_ = true;
      grab_offset_ = along - leading;
      invalidate_paint();
      return true;
    case pointer_action::move:
      if (!dragging_) {
        return false;
      }
      owner_->move_divider_to(along - grab_offset_);
      return true;
    case pointer_action::up:
    case pointer_action::cancel:
      dragging_ = false;
      invalidate_paint();
      return true;
    default:
      return false;
    }
  }
  auto const *const k = std::get_if<key_event>(&e);
  if (k == nullptr || !k->pressed || k->modifiers != modifier::none) {
    return false;
  }
  key const back = direction_ == axis::horizontal ? key::left : key::up;
  key const forward = direction_ == axis::horizontal ? key::right : key::down;
  float const step = resolve(key_step);
  if (k->code == back) {
    owner_->move_divider_by(-step);
  } else if (k->code == forward) {
    owner_->move_divider_by(step);
  } else if (k->code == key::home) {
    owner_->move_divider_to(-std::numeric_limits<float>::max());
  } else if (k->code == key::end) {
    owner_->move_divider_to(std::numeric_limits<float>::max());
  } else {
    return false;
  }
  (void)context;
  return true;
}

auto splitter::on_paint(render_context &context) const -> void {
  if (tree() == nullptr) {
    return;
  }
  auto const &colors = theme().colors;
  bool const active = dragging_ || interaction().hovered || interaction().pressed;
  float const thickness = std::max(device_stroke_width(owner_->divider_thickness(), scale()),
                                   active ? device_stroke_width(fixed(2), scale()) : 0.0f);
  basalt::size const extent = bounds().extent;
  float const across = main_extent(extent, direction_);
  float const start = snap_to_device((across - thickness) / 2, scale());
  rect const line = direction_ == axis::horizontal ? rect{{start, 0}, {thickness, extent.height}}
                                                   : rect{{0, start}, {extent.width, thickness}};
  context.fill_rect(line, active ? colors.border_accent : colors.border_subdued);
}

} // namespace basalt
