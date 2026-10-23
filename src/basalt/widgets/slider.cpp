#include "basalt/widgets/slider.h"

#include "basalt/event_router.h"
#include "basalt/render_context.h"
#include "basalt/widget_tree.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <variant>

#include "notify.h"

namespace basalt {

namespace {

constexpr length track_thickness = fixed(4);
constexpr length thumb_diameter = fixed(16);
constexpr length row_gap = fixed(4);
constexpr length minimum_width = fixed(120);
// Page Up and Page Down move by this many steps' worth of the range.
constexpr float page_share = 0.1f;

} // namespace

struct slider::metrics {
  basalt::size label;
  basalt::size value;
  float row_height = 0;
  float thumb = 0;
  float gap = 0;
};

slider::slider(float minimum, float maximum, float step, float value, control_size size)
    : minimum_(minimum), maximum_(maximum), step_(step), value_(value),
      label_({}, label_role_for(size)), value_line_({}, label_role_for(size)) {
  set_focus_policy(focus_policy::strong);
  set_minimum_target(true);
  set_range(minimum, maximum, step);
  set_value(value);
}

auto slider::set_range(float minimum, float maximum, float step) -> void {
  minimum_ = minimum;
  maximum_ = std::max(minimum, maximum);
  step_ = step > 0 ? step : (maximum_ - minimum_) / 1000;
  if (step_ <= 0) {
    step_ = 1;
  }
  float const next = snap(value_);
  if (next != value_) {
    value_ = next;
  }
  invalidate_content();
}

auto slider::snap(float value) const -> float {
  float const clamped = std::clamp(value, minimum_, maximum_);
  float const snapped = minimum_ + std::round((clamped - minimum_) / step_) * step_;
  return std::clamp(snapped, minimum_, maximum_);
}

auto slider::set_value(float value) -> void {
  float const next = snap(value);
  if (next == value_) {
    return;
  }
  value_ = next;
  invalidate_content();
}

auto slider::set_label(std::string label) -> void {
  if (label_.set_text(std::move(label))) {
    invalidate_content();
  }
}

auto slider::set_value_formatter(std::function<std::string(float)> formatter) -> void {
  formatter_ = std::move(formatter);
  invalidate_content();
}

auto slider::value_text() const -> std::string { return formatter_ ? formatter_(value_) : ""; }

auto slider::measure_parts() const -> metrics {
  metrics m;
  value_line_.set_text(value_text());
  m.label = label_.text().empty() ? basalt::size{} : label_.measure(*this);
  m.value = value_line_.text().empty() ? basalt::size{} : value_line_.measure(*this);
  m.row_height = std::max(m.label.height, m.value.height);
  m.thumb = resolve(thumb_diameter);
  m.gap = m.row_height > 0 ? resolve(row_gap) : 0;
  return m;
}

auto slider::track_rect() const -> rect {
  metrics const m = measure_parts();
  float const thickness = resolve(track_thickness);
  float const center = m.row_height + m.gap + m.thumb / 2;
  float const edge = m.thumb / 2;
  return {{edge, center - thickness / 2}, {std::max(0.0f, bounds().width() - 2 * edge), thickness}};
}

auto slider::thumb_center() const -> point {
  rect const track = track_rect();
  float const span = maximum_ - minimum_;
  float const fraction = span > 0 ? (value_ - minimum_) / span : 0;
  return {track.origin.x + track.width() * fraction, track.origin.y + track.height() / 2};
}

auto slider::value_at(float x) const -> float {
  rect const track = track_rect();
  if (track.width() <= 0) {
    return minimum_;
  }
  float const fraction = std::clamp((x - track.origin.x) / track.width(), 0.0f, 1.0f);
  return snap(minimum_ + fraction * (maximum_ - minimum_));
}

auto slider::on_measure(constraints const &) const -> basalt::size {
  metrics const m = measure_parts();
  float const row_width = m.label.width + m.value.width + (m.value.width > 0 ? m.thumb : 0);
  return {std::max(resolve(minimum_width), row_width), m.row_height + m.gap + m.thumb};
}

auto slider::apply(float value, bool final) -> void {
  float const next = snap(value);
  if (next != value_) {
    value_ = next;
    invalidate_content();
    notify(on_change_, next);
  }
  if (final) {
    notify(on_commit_, next);
  }
}

auto slider::handle_key(key code, modifier modifiers) -> bool {
  if (modifiers != modifier::none) {
    return false;
  }
  float const page = std::max(step_, (maximum_ - minimum_) * page_share);
  switch (code) {
  case key::left:
  case key::down:
    apply(value_ - step_, true);
    return true;
  case key::right:
  case key::up:
    apply(value_ + step_, true);
    return true;
  case key::page_down:
    apply(value_ - page, true);
    return true;
  case key::page_up:
    apply(value_ + page, true);
    return true;
  case key::home:
    apply(minimum_, true);
    return true;
  case key::end:
    apply(maximum_, true);
    return true;
  default:
    return false;
  }
}

auto slider::on_event(event_context &, event const &e) -> bool {
  if (auto const *k = std::get_if<key_event>(&e)) {
    if (!k->pressed) {
      return false;
    }
    if (k->code == key::escape && dragging_) {
      dragging_ = false;
      apply(drag_start_value_, false);
      return true;
    }
    return handle_key(k->code, k->modifiers);
  }
  auto const *p = std::get_if<pointer_event>(&e);
  if (p == nullptr) {
    return false;
  }
  switch (p->action) {
  case pointer_action::down:
    if (p->button != pointer_button::primary) {
      return false;
    }
    dragging_ = true;
    drag_start_value_ = value_;
    apply(value_at(p->position.x), false);
    invalidate_paint();
    return true;
  case pointer_action::move:
    if (!dragging_) {
      return false;
    }
    apply(value_at(p->position.x), false);
    return true;
  case pointer_action::up:
    if (!dragging_ || p->button != pointer_button::primary) {
      return false;
    }
    dragging_ = false;
    apply(value_at(p->position.x), true);
    invalidate_paint();
    return true;
  case pointer_action::cancel:
    if (!dragging_) {
      return false;
    }
    dragging_ = false;
    apply(drag_start_value_, false);
    invalidate_paint();
    return true;
  default:
    return false;
  }
}

auto slider::on_paint(render_context &context) const -> void {
  if (tree() == nullptr) {
    return;
  }
  basalt::theme const &t = theme();
  auto const &c = t.colors;
  metrics const m = measure_parts();
  float const width = bounds().width();
  bool const on = enabled();
  if (m.label.width > 0) {
    label_.draw_in(context, *this, {{0, 0}, {m.label.width, m.row_height}},
                   on ? c.text_default : c.text_subdued, alignment::start, alignment::center);
  }
  if (m.value.width > 0) {
    value_line_.draw_in(context, *this, {{width - m.value.width, 0}, {m.value.width, m.row_height}},
                        c.text_subdued, alignment::end, alignment::center);
  }

  rect const track = track_rect();
  float const radius = track.height() / 2;
  point const center = thumb_center();
  context.fill_rounded_rect(track, radius, c.background_tertiary);
  rect const filled{track.origin, {center.x - track.origin.x, track.height()}};
  if (filled.width() > 0) {
    context.fill_rounded_rect(filled, radius, on ? c.border_accent : c.border_default);
  }
  control_state const state =
      dragging_ ? control_state::pressed : control_state_of(interaction(), enabled());
  rgba const thumb_fill = !on ? c.icon_subdued
                              : by_state(state, c.background_accent, c.background_accent_hover,
                                         c.background_accent_active);
  context.fill_rounded_rect({{center.x - m.thumb / 2, center.y - m.thumb / 2}, {m.thumb, m.thumb}},
                            m.thumb / 2, thumb_fill);
}

auto slider::accessibility_info() const -> accessibility::info {
  accessibility::info details;
  details.kind = accessibility::role::slider;
  details.name = label_.text();
  details.value = value_text();
  details.orientation = accessibility::orientation::horizontal;
  details.range = accessibility::value_range{minimum_, maximum_, step_};
  details.numeric_value = value_;
  details.actions = {accessibility::action::increment, accessibility::action::decrement,
                     accessibility::action::focus};
  return details;
}

auto slider::on_accessibility_action(accessibility::action_request const &request) -> bool {
  switch (request.kind) {
  case accessibility::action::increment:
    apply(value_ + step_, true);
    return true;
  case accessibility::action::decrement:
    apply(value_ - step_, true);
    return true;
  default:
    return false;
  }
}

} // namespace basalt
