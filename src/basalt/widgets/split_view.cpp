#include "basalt/widgets/split_view.h"

#include "basalt/render_context.h"
#include "basalt/widget_tree.h"

#include <algorithm>
#include <utility>

namespace basalt {

auto split_view::find_child(widget_id id) const -> widget * {
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

auto split_view::first() const -> widget * { return find_child(first_id_); }
auto split_view::second() const -> widget * { return find_child(second_id_); }

auto split_view::replace_pane(widget_id &slot, std::size_t index, std::unique_ptr<widget> pane)
    -> void {
  if (widget *const old = find_child(slot)) {
    // Destroyed at the end of this statement.
    (void)remove_child(*old);
  }
  // The splitter stays in front of the panes.
  std::size_t const limit = child_count() - (find_child(handle_id_) != nullptr ? 1 : 0);
  slot = insert_child(std::min(index, limit), std::move(pane)).id();
}

auto split_view::handle() const -> splitter * {
  return static_cast<splitter *>(find_child(handle_id_));
}

auto split_view::set_resizable(bool resizable) -> void {
  if (resizable == resizable_) {
    return;
  }
  resizable_ = resizable;
  if (resizable) {
    handle_id_ = emplace_child<splitter>(*this, direction_).id();
  } else if (splitter *const h = handle()) {
    (void)remove_child(*h);
  }
  invalidate_content();
}

auto split_view::on_paint(render_context &context) const -> void {
  if (resizable_ || divider_.empty() || tree() == nullptr) {
    return;
  }
  context.fill_rect(divider_, theme().colors.border_subdued);
}

auto split_view::on_child_removed(widget &child) -> void {
  if (child.id() == first_id_) {
    first_id_ = {};
  } else if (child.id() == second_id_) {
    second_id_ = {};
  } else if (child.id() == handle_id_) {
    handle_id_ = {};
    resizable_ = false;
  }
}

auto split_view::set_direction(axis direction) -> void {
  if (direction != direction_) {
    direction_ = direction;
    if (splitter *const h = handle()) {
      h->set_direction(direction);
    }
    invalidate_layout();
  }
}

auto split_view::set_divider_thickness(length thickness) -> void {
  if (thickness != divider_thickness_) {
    divider_thickness_ = thickness;
    invalidate_layout();
  }
}

auto split_view::set_minimums(length first, length second) -> void {
  if (first != first_minimum_ || second != second_minimum_) {
    first_minimum_ = first;
    second_minimum_ = second;
    invalidate_layout();
  }
}

auto split_view::set_ratio(float ratio) -> void {
  anchor_ = split_anchor::proportional;
  ratio_ = std::clamp(ratio, 0.0f, 1.0f);
  invalidate_layout();
}

auto split_view::set_first_length(length pane_length) -> void {
  anchor_ = split_anchor::first;
  anchored_ = pane_length;
  invalidate_layout();
}

auto split_view::set_second_length(length pane_length) -> void {
  anchor_ = split_anchor::second;
  anchored_ = pane_length;
  invalidate_layout();
}

auto split_view::available() const -> float {
  return std::max(0.0f, main_extent(bounds().extent, direction_) - resolve(divider_thickness_));
}

auto split_view::limit_first(float first, float space) const -> float {
  first = std::min(first, space - resolve(second_minimum_));
  first = std::max(first, resolve(first_minimum_));
  return std::clamp(first, 0.0f, space);
}

auto split_view::desired_first(float space) const -> float {
  switch (anchor_) {
  case split_anchor::first:
    return resolve(anchored_);
  case split_anchor::second:
    return space - resolve(anchored_);
  case split_anchor::proportional:
    break;
  }
  return ratio_ * space;
}

auto split_view::move_divider_to(float position) -> float {
  float const space = available();
  float const first = limit_first(position, space);
  switch (anchor_) {
  case split_anchor::proportional:
    if (space > 0) {
      ratio_ = first / space;
    }
    break;
  case split_anchor::first:
    anchored_ = from_layout(first, anchored_.kind, scale());
    break;
  case split_anchor::second:
    anchored_ = from_layout(space - first, anchored_.kind, scale());
    break;
  }
  invalidate_layout();
  return snap_to_device(first, scale());
}

auto split_view::move_divider_by(float delta) -> float {
  float const space = available();
  return move_divider_to(limit_first(desired_first(space), space) + delta);
}

auto split_view::on_measure(constraints const &limits) const -> basalt::size {
  constraints const each{{},
                         make_size(direction_, unbounded, cross_extent(limits.max, direction_))};
  widget const *const panes[] = {first(), second()};
  length const minimums[] = {first_minimum_, second_minimum_};
  float main = 0;
  float cross = 0;
  int shown = 0;
  for (int i = 0; i < 2; ++i) {
    if (panes[i] == nullptr || !panes[i]->visible()) {
      continue;
    }
    basalt::size const measured = panes[i]->measure(each);
    main += std::max(main_extent(measured, direction_), resolve(minimums[i]));
    cross = std::max(cross, cross_extent(measured, direction_));
    ++shown;
  }
  if (shown == 2) {
    main += resolve(divider_thickness_);
  }
  return make_size(direction_, main, cross);
}

auto split_view::on_layout() -> void {
  basalt::size const extent = bounds().extent;
  float const cross = cross_extent(extent, direction_);
  scale_factors const factors = scale();
  widget *const a = first();
  widget *const b = second();
  bool const show_a = a != nullptr && a->visible();
  bool const show_b = b != nullptr && b->visible();

  splitter *const grip = handle();
  if (grip != nullptr) {
    grip->set_visible(show_a && show_b);
  }
  if (!(show_a && show_b)) {
    divider_ = {};
    if (show_a || show_b) {
      (show_a ? a : b)->set_bounds({{}, extent});
    }
    return;
  }

  float const space = available();
  float const first_end = snap_to_device(limit_first(desired_first(space), space), factors);
  float const second_start = snap_to_device(first_end + resolve(divider_thickness_), factors);
  float const end = main_extent(extent, direction_);
  auto const span = [&](float from, float to) {
    return rect{make_point(direction_, from, 0),
                make_size(direction_, std::max(0.0f, to - from), cross)};
  };
  a->set_bounds(span(0, first_end));
  divider_ = span(first_end, second_start);
  b->set_bounds(span(second_start, std::max(second_start, end)));
  if (grip != nullptr) {
    // The grab area is centred on the divider and may overlap the panes.
    float const grab = std::max(resolve(minimum_grab_width), second_start - first_end);
    float const start = first_end - (grab - (second_start - first_end)) / 2;
    grip->set_bounds(span(snap_to_device(start, factors), snap_to_device(start + grab, factors)));
  }
}

} // namespace basalt
