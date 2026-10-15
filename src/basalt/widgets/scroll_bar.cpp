#include "basalt/widgets/scroll_bar.h"

#include "basalt/event_router.h"
#include "basalt/render_context.h"
#include "basalt/widget_tree.h"

#include <algorithm>
#include <utility>
#include <variant>

#include "notify.h"

namespace basalt {

auto scroll_bar::set_metrics(float content, float viewport, float offset) -> void {
  if (content == content_ && viewport == viewport_ && offset == offset_) {
    return;
  }
  content_ = content;
  viewport_ = viewport;
  offset_ = offset;
  invalidate_paint();
}

auto scroll_bar::max_offset() const -> float { return std::max(0.0f, content_ - viewport_); }

auto scroll_bar::track_length() const -> float { return main_extent(bounds().extent, direction_); }

auto scroll_bar::thumb_length() const -> float {
  float const track = track_length();
  if (content_ <= 0 || max_offset() <= 0) {
    return 0;
  }
  return std::min(track, std::max(resolve(minimum_thumb), track * viewport_ / content_));
}

auto scroll_bar::thumb_start() const -> float {
  float const travel = track_length() - thumb_length();
  float const most = max_offset();
  return most > 0 ? travel * std::clamp(offset_, 0.0f, most) / most : 0;
}

auto scroll_bar::thumb_rect() const -> rect {
  float const length = thumb_length();
  if (length <= 0) {
    return {};
  }
  basalt::size const extent = bounds().extent;
  return {make_point(direction_, thumb_start(), 0),
          make_size(direction_, length, cross_extent(extent, direction_))};
}

auto scroll_bar::request_offset(float offset) -> void {
  float const next = std::clamp(offset, 0.0f, max_offset());
  if (next != offset_) {
    notify(on_scroll_, next);
  }
}

auto scroll_bar::accessibility_info() const -> accessibility::info {
  accessibility::info details;
  details.kind = accessibility::role::scroll_bar;
  details.name = direction_ == axis::vertical ? "Vertical scroll bar" : "Horizontal scroll bar";
  details.orientation = direction_ == axis::vertical ? accessibility::orientation::vertical
                                                     : accessibility::orientation::horizontal;
  details.range = accessibility::value_range{0, max_offset(), std::max(1.0f, viewport_ / 10)};
  details.numeric_value = offset_;
  details.actions = {accessibility::action::increment, accessibility::action::decrement};
  return details;
}

auto scroll_bar::on_accessibility_action(accessibility::action_request const &request) -> bool {
  float const step = std::max(1.0f, viewport_ / 10);
  switch (request.kind) {
  case accessibility::action::increment:
    request_offset(offset_ + step);
    return true;
  case accessibility::action::decrement:
    request_offset(offset_ - step);
    return true;
  default:
    return false;
  }
}

auto scroll_bar::on_event(event_context &, event const &e) -> bool {
  auto const *const p = std::get_if<pointer_event>(&e);
  if (p == nullptr) {
    return false;
  }
  float const along = main_coordinate(p->position, direction_);
  switch (p->action) {
  case pointer_action::down: {
    if (p->button != pointer_button::primary || thumb_length() <= 0) {
      return false;
    }
    rect const thumb = thumb_rect();
    float const start = main_coordinate(thumb.origin, direction_);
    float const length = main_extent(thumb.extent, direction_);
    if (along >= start && along < start + length) {
      dragging_ = true;
      drag_origin_ = along;
      drag_offset_ = offset_;
    } else {
      float const page = std::max(1.0f, viewport_ * 0.9f);
      request_offset(along < start ? offset_ - page : offset_ + page);
    }
    return true;
  }
  case pointer_action::move: {
    if (!dragging_) {
      return false;
    }
    float const travel = track_length() - thumb_length();
    if (travel > 0) {
      request_offset(drag_offset_ + (along - drag_origin_) * max_offset() / travel);
    }
    return true;
  }
  case pointer_action::up:
  case pointer_action::cancel:
    dragging_ = false;
    return true;
  default:
    return false;
  }
}

auto scroll_bar::on_paint(render_context &context) const -> void {
  rect const thumb = thumb_rect();
  if (tree() == nullptr || thumb.empty()) {
    return;
  }
  auto const &colors = theme().colors;
  rgba const fill = dragging_ || interaction().pressed ? colors.icon_default
                    : interaction().hovered            ? colors.icon_neutral_default
                                                       : colors.icon_subdued;
  rect const drawn = inset(thumb, direction_ == axis::vertical ? insets::symmetric(2, 0)
                                                               : insets::symmetric(0, 2));
  float const radius = std::min(drawn.width(), drawn.height()) / 2;
  context.fill_rounded_rect(drawn, radius, fill);
}

} // namespace basalt
