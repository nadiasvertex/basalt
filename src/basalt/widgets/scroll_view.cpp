#include "basalt/widgets/scroll_view.h"

#include "basalt/event_router.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <utility>
#include <variant>

namespace basalt {

namespace {

// The offset along one axis that shows [start, start + extent) while moving
// as little as possible from `offset`.
auto reveal(float start, float extent, float offset, float view) -> float {
  if (extent > view || start < offset) {
    return start;
  }
  if (start + extent > offset + view) {
    return start + extent - view;
  }
  return offset;
}

// How quickly a wheel glide closes on its destination: the remaining distance
// shrinks by a factor of e every this many seconds. About a quarter of a second
// to settle feels immediate while the first frames still move far.
constexpr float glide_time_constant = 0.06f;
// A glide ends when it is this close to its destination, in layout pixels.
constexpr float glide_rest_distance = 0.5f;

} // namespace

auto scroll_view::find_child(widget_id id) const -> widget * {
  if (!id.valid()) {
    return nullptr;
  }
  for (auto &c : children()) {
    if (c.id() == id) {
      return &c;
    }
  }
  return nullptr;
}

auto scroll_view::content() const -> widget * { return find_child(content_id_); }

auto scroll_view::replace_content(std::unique_ptr<widget> content) -> void {
  if (widget *const old = this->content()) {
    // Destroyed at the end of this statement.
    (void)remove_child(*old);
  }
  content_id_ = insert_child(0, std::move(content)).id();
  content_size_ = {};
  offset_ = {};
  gliding_ = false;
}

auto scroll_view::on_child_removed(widget &child) -> void {
  if (child.id() == content_id_) {
    content_id_ = {};
    content_size_ = {};
    offset_ = {};
    gliding_ = false;
  } else if (child.id() == vertical_bar_id_) {
    vertical_bar_id_ = {};
  } else if (child.id() == horizontal_bar_id_) {
    horizontal_bar_id_ = {};
  }
}

auto scroll_view::scrollbar(axis a) const -> scroll_bar * {
  return static_cast<scroll_bar *>(
      find_child(a == axis::vertical ? vertical_bar_id_ : horizontal_bar_id_));
}

auto scroll_view::set_scrollbars(bool enabled) -> void {
  if (enabled == scrollbars_) {
    return;
  }
  scrollbars_ = enabled;
  if (!enabled) {
    for (widget_id const id : {vertical_bar_id_, horizontal_bar_id_}) {
      if (widget *const bar = find_child(id)) {
        (void)remove_child(*bar);
      }
    }
    return;
  }
  for (axis const a : {axis::vertical, axis::horizontal}) {
    auto &bar = emplace_child<scroll_bar>(a);
    bar.set_on_scroll([this, a](float offset) {
      set_scroll_offset(a == axis::vertical ? point{offset_.x, offset} : point{offset, offset_.y});
    });
    (a == axis::vertical ? vertical_bar_id_ : horizontal_bar_id_) = bar.id();
  }
  invalidate_layout();
}

auto scroll_view::on_event(event_context &, event const &e) -> bool {
  float const line = resolve(fixed(40));
  if (auto const *w = std::get_if<wheel_event>(&e)) {
    point delta = w->precise ? w->delta : point{w->delta.x * line, w->delta.y * line};
    if (has(w->modifiers, modifier::shift) && delta.x == 0) {
      delta = {delta.y, 0};
    }
    return glide_by(
        {scrolls(axis::horizontal) ? delta.x : 0, scrolls(axis::vertical) ? delta.y : 0});
  }
  auto const *const k = std::get_if<key_event>(&e);
  if (k == nullptr || !k->pressed || k->modifiers != modifier::none || !has_focus()) {
    return false;
  }
  float const page = std::max(line, viewport_size().height - line);
  point const most = max_scroll_offset();
  switch (k->code) {
  case key::up:
    return scrolls(axis::vertical) && scroll_by({0, -line});
  case key::down:
    return scrolls(axis::vertical) && scroll_by({0, line});
  case key::left:
    return scrolls(axis::horizontal) && scroll_by({-line, 0});
  case key::right:
    return scrolls(axis::horizontal) && scroll_by({line, 0});
  case key::page_up:
    return scrolls(axis::vertical) && scroll_by({0, -page});
  case key::page_down:
    return scrolls(axis::vertical) && scroll_by({0, page});
  case key::home:
    return set_scroll_offset(scrolls(axis::vertical) ? point{offset_.x, 0} : point{0, offset_.y});
  case key::end:
    return set_scroll_offset(scrolls(axis::vertical) ? point{offset_.x, most.y}
                                                     : point{most.x, offset_.y});
  default:
    return false;
  }
}

auto scroll_view::set_axes(scroll_axes axes) -> void {
  if (axes != axes_) {
    axes_ = axes;
    invalidate_layout();
  }
}

auto scroll_view::scrolls(axis a) const -> bool {
  return a == axis::horizontal ? axes_ != scroll_axes::vertical : axes_ != scroll_axes::horizontal;
}

auto scroll_view::max_scroll_offset() const -> point {
  basalt::size const view = viewport_size();
  return {std::max(0.0f, content_size_.width - view.width),
          std::max(0.0f, content_size_.height - view.height)};
}

auto scroll_view::clamped(point offset) const -> point {
  scale_factors const factors = scale();
  point const most = max_scroll_offset();
  return {std::clamp(snap_to_device(offset.x, factors), 0.0f, most.x),
          std::clamp(snap_to_device(offset.y, factors), 0.0f, most.y)};
}

auto scroll_view::set_scroll_offset(point offset) -> bool {
  gliding_ = false;
  return apply_offset(offset);
}

auto scroll_view::apply_offset(point offset) -> bool {
  point next;
  if (needs_layout()) {
    // The content size is about to change; clamp in `on_layout`.
    scale_factors const factors = scale();
    next = {scrolls(axis::horizontal) ? std::max(0.0f, snap_to_device(offset.x, factors)) : 0,
            scrolls(axis::vertical) ? std::max(0.0f, snap_to_device(offset.y, factors)) : 0};
  } else {
    next = clamped(offset);
  }
  if (next == offset_) {
    return false;
  }
  offset_ = next;
  if (!needs_layout()) {
    place_content();
    if (widget *const c = content()) {
      c->on_viewport_moved();
    }
  }
  return true;
}

auto scroll_view::scroll_by(point delta) -> bool { return set_scroll_offset(offset_ + delta); }

auto scroll_view::glide_by(point delta) -> bool {
  if (needs_layout()) {
    // The content size is unknown, so there is nothing to ease toward yet.
    return scroll_by(delta);
  }
  point const from = gliding_ ? glide_target_ : offset_;
  point const most = max_scroll_offset();
  // The destination is not snapped to device pixels, so that small deltas add up.
  point const to{std::clamp(from.x + delta.x, 0.0f, most.x),
                 std::clamp(from.y + delta.y, 0.0f, most.y)};
  if (to == from) {
    return false;
  }
  glide_target_ = to;
  gliding_ = true;
  request_animation_frame();
  return true;
}

auto scroll_view::on_animation_frame(std::chrono::nanoseconds elapsed) -> bool {
  if (!gliding_) {
    return false;
  }
  point const target = clamped(glide_target_);
  float const seconds = std::chrono::duration<float>(elapsed).count();
  float const closed = 1.0f - std::exp(-seconds / glide_time_constant);
  point const next{offset_.x + (target.x - offset_.x) * closed,
                   offset_.y + (target.y - offset_.y) * closed};
  bool const moved = apply_offset(next);
  bool const near = std::abs(target.x - offset_.x) <= glide_rest_distance &&
                    std::abs(target.y - offset_.y) <= glide_rest_distance;
  if (moved && !near) {
    return true;
  }
  // Either the glide arrived, or each step is now too small to change a device
  // pixel; finish on the destination.
  apply_offset(target);
  gliding_ = false;
  return false;
}

auto scroll_view::scroll_into_view(rect area) -> bool {
  basalt::size const view = viewport_size();
  point target = offset_;
  if (scrolls(axis::horizontal)) {
    target.x = reveal(area.left(), area.width(), offset_.x, view.width);
  }
  if (scrolls(axis::vertical)) {
    target.y = reveal(area.top(), area.height(), offset_.y, view.height);
  }
  return set_scroll_offset(target);
}

auto scroll_view::content_limits(basalt::size view) const -> constraints {
  return {view,
          {scrolls(axis::horizontal) ? unbounded : view.width,
           scrolls(axis::vertical) ? unbounded : view.height}};
}

auto scroll_view::on_measure(constraints const &limits) const -> basalt::size {
  widget const *const c = content();
  if (c == nullptr) {
    return limits.min;
  }
  return c->measure({{},
                     {scrolls(axis::horizontal) ? unbounded : limits.max.width,
                      scrolls(axis::vertical) ? unbounded : limits.max.height}});
}

auto scroll_view::on_layout() -> void {
  widget *const c = content();
  if (c == nullptr) {
    content_size_ = {};
    offset_ = {};
    gliding_ = false;
    update_bars();
    return;
  }
  content_size_ = c->measure(content_limits(viewport_size()));
  offset_ = clamped(offset_);
  if (gliding_) {
    glide_target_ = clamped(glide_target_);
  }
  place_content();
}

auto scroll_view::place_content() -> void {
  if (widget *const c = content()) {
    c->set_bounds({{-offset_.x, -offset_.y}, content_size_});
  }
  update_bars();
}

auto scroll_view::update_bars() -> void {
  if (!scrollbars_) {
    return;
  }
  basalt::size const view = viewport_size();
  point const most = max_scroll_offset();
  float const thickness = resolve(scroll_bar::default_thickness);
  bool const need_vertical = scrolls(axis::vertical) && most.y > 0;
  bool const need_horizontal = scrolls(axis::horizontal) && most.x > 0;
  if (scroll_bar *const bar = scrollbar(axis::vertical)) {
    bar->set_visible(need_vertical);
    bar->set_bounds({{view.width - thickness, 0},
                     {thickness, view.height - (need_horizontal ? thickness : 0)}});
    bar->set_metrics(content_size_.height, view.height, offset_.y);
  }
  if (scroll_bar *const bar = scrollbar(axis::horizontal)) {
    bar->set_visible(need_horizontal);
    bar->set_bounds(
        {{0, view.height - thickness}, {view.width - (need_vertical ? thickness : 0), thickness}});
    bar->set_metrics(content_size_.width, view.width, offset_.x);
  }
}

} // namespace basalt
