#include "basalt/widgets/tabs.h"

#include "basalt/event_router.h"
#include "basalt/widget_tree.h"

#include <algorithm>
#include <cmath>
#include <string_view>
#include <utility>
#include <variant>

namespace basalt {

namespace {

auto theme_of(widget const &w) -> theme const & {
  static theme const fallback;
  return w.tree() != nullptr ? w.tree()->theme() : fallback;
}

// Byte offset after the first `count` code points of `text`.
auto prefix_end(std::string_view text, std::size_t count) -> std::size_t {
  std::size_t at = 0;
  while (count > 0 && at < text.size()) {
    ++at;
    while (at < text.size() && (static_cast<unsigned char>(text[at]) & 0xC0) == 0x80) {
      ++at;
    }
    --count;
  }
  return at;
}

// Byte offset where the last `count` code points of `text` begin.
auto suffix_begin(std::string_view text, std::size_t count) -> std::size_t {
  std::size_t at = text.size();
  while (count > 0 && at > 0) {
    --at;
    while (at > 0 && (static_cast<unsigned char>(text[at]) & 0xC0) == 0x80) {
      --at;
    }
    --count;
  }
  return at;
}

auto code_point_count(std::string_view text) -> std::size_t {
  std::size_t count = 0;
  for (char const c : text) {
    if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) {
      ++count;
    }
  }
  return count;
}

// Layout pixels at the default scale. Tabs shrink to the minimum before the row scrolls.
constexpr float min_tab_width = 96;
constexpr float max_tab_width = 280;
// Part of the neighbouring tab left visible when a tab is scrolled into view.
constexpr float reveal_margin = 32;
// Pixels moved by one wheel notch.
constexpr float wheel_step = 48;

// The offset that shows [left, right] with `margin` to spare, moving as little as possible from
// `offset`. A tab too wide to show with its margins is left alone.
auto revealed(float left, float right, float offset, float view, float margin) -> float {
  if (right - left + 2 * margin >= view) {
    return offset;
  }
  if (left - margin < offset) {
    return std::max(0.0f, left - margin);
  }
  if (right + margin > offset + view) {
    return right + margin - view;
  }
  return offset;
}

} // namespace

tab::tab(std::string label) : line_(std::move(label), text_role::body_strong) {
  set_focus_policy(focus_policy::strong);
  set_minimum_target(true);
}

auto tab::set_label(std::string label) -> void {
  if (line_.set_text(std::move(label))) {
    invalidate_layout();
    invalidate_paint();
  }
}

auto tab::set_selected(bool selected) -> void {
  if (selected != selected_) {
    selected_ = selected;
    invalidate_paint();
  }
}

auto tab::request_selection() -> void {
  if (auto *const bar = dynamic_cast<tab_bar *>(parent())) {
    if (auto const index = bar->index_of(*this)) {
      bar->select(*index);
    }
  }
}

auto tab::set_closable(bool closable) -> void {
  if (closable != closable_) {
    closable_ = closable;
    invalidate_layout();
    invalidate_paint();
  }
}

auto tab::request_close() -> void {
  if (auto *const bar = dynamic_cast<tab_bar *>(parent())) {
    if (auto const index = bar->index_of(*this)) {
      auto const handler = bar->on_close_;
      if (handler) {
        // The handler may destroy this tab; do not touch members afterwards.
        handler(*index);
      }
    }
  }
}

auto tab::close_area() const -> rect {
  auto const &sizes = theme_of(*this).sizes;
  float const side = resolve(fixed(16));
  float const edge = resolve(sizes.inset_horizontal_tab);
  return {{bounds().width() - edge - side, (bounds().height() - side) / 2}, {side, side}};
}

auto tab::over_close(point local) const -> bool {
  if (!closable_) {
    return false;
  }
  return close_area().contains(local);
}

auto tab::fitted_label(float available) const -> text_line const & {
  float const natural = line_.measure(*this).width;
  if (natural <= available) {
    return line_;
  }
  if (fitted_width_ == available && fitted_natural_ == natural && fitted_source_ == line_.text()) {
    return fitted_line_;
  }
  fitted_width_ = available;
  fitted_natural_ = natural;
  fitted_source_ = line_.text();
  std::string_view const full = fitted_source_;
  auto const build = [&](std::size_t kept) {
    std::size_t const head = (kept + 1) / 2;
    std::size_t const tail = kept / 2;
    std::string out{full.substr(0, prefix_end(full, head))};
    out += "\u2026";
    out += full.substr(suffix_begin(full, tail));
    return out;
  };
  fitted_line_.set_role(line_.role());
  // Find the most code points that still fit alongside the ellipsis.
  std::size_t low = 0;
  std::size_t high = code_point_count(full);
  while (low < high) {
    std::size_t const mid = (low + high + 1) / 2;
    fitted_line_.set_text(build(mid));
    if (fitted_line_.measure(*this).width <= available) {
      low = mid;
    } else {
      high = mid - 1;
    }
  }
  fitted_line_.set_text(build(low));
  return fitted_line_;
}

auto tab::accessibility_info() const -> accessibility::info {
  accessibility::info details;
  details.kind = accessibility::role::tab;
  details.name = line_.text();
  details.selected = selected_;
  details.actions = {accessibility::action::press, accessibility::action::select,
                     accessibility::action::focus};
  if (parent() != nullptr) {
    if (auto const index = parent()->index_of(*this)) {
      details.position_in_set = static_cast<std::uint32_t>(*index + 1);
      details.set_size = static_cast<std::uint32_t>(parent()->child_count());
    }
  }
  return details;
}

auto tab::on_accessibility_action(accessibility::action_request const &request) -> bool {
  if (request.kind == accessibility::action::press ||
      request.kind == accessibility::action::select) {
    request_selection();
    return true;
  }
  return false;
}

auto tab::on_interaction_changed(interaction_state const &previous) -> void {
  widget::on_interaction_changed(previous);
  if (interaction().focused && !previous.focused) {
    request_selection();
  }
}

auto tab::on_event(event_context &, event const &e) -> bool {
  if (auto const *p = std::get_if<pointer_event>(&e)) {
    if (closable_) {
      bool const over = p->action != pointer_action::leave && over_close(p->position);
      if (over != close_hovered_) {
        close_hovered_ = over;
        invalidate_paint();
      }
    }
    if (p->action == pointer_action::down && p->button == pointer_button::primary &&
        over_close(p->position)) {
      close_pressed_ = true;
      return true;
    }
    if (p->action == pointer_action::up && p->button == pointer_button::primary && close_pressed_) {
      close_pressed_ = false;
      if (over_close(p->position)) {
        request_close();
      }
      return true;
    }
    if (p->button != pointer_button::primary) {
      return false;
    }
    if (p->action == pointer_action::down) {
      return true;
    }
    if (p->action == pointer_action::up) {
      if (interaction().pressed) {
        request_selection();
      }
      return true;
    }
    return false;
  }
  if (auto const *k = std::get_if<key_event>(&e)) {
    if (k->pressed && k->modifiers == modifier::none &&
        (k->code == key::enter || k->code == key::space)) {
      request_selection();
      return true;
    }
  }
  return false;
}

auto tab::on_measure(constraints const &) const -> basalt::size {
  auto const &sizes = theme_of(*this).sizes;
  float const close = closable_ ? resolve(fixed(16)) + resolve(fixed(4)) : 0.0f;
  return {line_.measure(*this).width + 2 * resolve(sizes.inset_horizontal_tab) + close,
          resolve(sizes.dimension_height_tab)};
}

auto tab::on_paint(render_context &context) const -> void {
  if (tree() == nullptr) {
    return;
  }
  theme const &t = theme_of(*this);
  rect const area{{}, bounds().extent};
  switch (control_state_of(interaction(), enabled())) {
  case control_state::hovered:
    context.fill_rect(area, t.colors.background_accent_transparent_hover);
    break;
  case control_state::pressed:
    context.fill_rect(area, t.colors.background_accent_transparent_active);
    break;
  case control_state::normal:
  case control_state::disabled:
    break;
  }
  rect text_area = area;
  float const inset = resolve(t.sizes.inset_horizontal_tab);
  text_area.origin.x += inset;
  text_area.extent.width = std::max(0.0f, text_area.extent.width - 2 * inset);
  if (closable_) {
    rect const close = close_area();
    text_area.extent.width = std::max(0.0f, close.left() - resolve(fixed(4)) - text_area.origin.x);
    if (close_hovered_) {
      context.fill_rounded_rect(close, resolve(t.sizes.radius_default),
                                t.colors.background_accent_transparent_active);
    }
    float const arm = close.width() * 0.25f;
    point const middle = close.center();
    float const width = device_stroke_width(fixed(1.5f), scale());
    rgba const color = close_hovered_ ? t.colors.text_default : t.colors.icon_default;
    context.line(middle - point{arm, arm}, middle + point{arm, arm}, color, width);
    context.line(middle - point{arm, -arm}, middle + point{arm, -arm}, color, width);
  }
  fitted_label(text_area.width())
      .draw_in(context, *this, text_area, selected_ ? t.colors.text_default : t.colors.text_subdued,
               alignment::center);
  if (selected_) {
    float const height = device_stroke_width(fixed(2), scale());
    context.fill_rect({{0, area.height() - height}, {area.width(), height}},
                      t.colors.border_accent);
  }
}

tab_bar::tab_bar(std::string name) : box(axis::horizontal), name_(std::move(name)) {
  set_focus_scope(focus_scope::group);
  set_cross_alignment(alignment::stretch);
}

auto tab_bar::add_tab(std::string label) -> tab & {
  auto &added = emplace_child<tab>(std::move(label));
  set_item(added, {.min = fixed(min_tab_width), .max = fixed(max_tab_width)});
  if (tab_count() == 1) {
    select(0);
  }
  return added;
}

auto tab_bar::tab_at(std::size_t index) const -> tab & { return static_cast<tab &>(child(index)); }

auto tab_bar::selected_index() const -> std::optional<std::size_t> {
  for (std::size_t i = 0; i < tab_count(); ++i) {
    if (tab_at(i).selected()) {
      return i;
    }
  }
  return std::nullopt;
}

auto tab_bar::select(std::size_t index) -> bool {
  if (index >= tab_count()) {
    return false;
  }
  auto const previous = selected_index();
  for (std::size_t i = 0; i < tab_count(); ++i) {
    tab_at(i).set_selected(i == index);
  }
  reveal_selected_ = true;
  invalidate_layout();
  if (tree() != nullptr) {
    // Tab re-enters the group at the selected tab.
    (void)tree()->focus().set_group_entry(*this, tab_at(index));
  }
  if (previous != index && on_select_) {
    auto const handler = on_select_;
    handler(index);
  }
  return true;
}

auto tab_bar::accessibility_info() const -> accessibility::info {
  accessibility::info details;
  details.kind = accessibility::role::tab_list;
  details.name = name_;
  details.orientation = accessibility::orientation::horizontal;
  return details;
}

auto tab_bar::max_scroll_offset() const -> float {
  return std::max(0.0f, content_width_ - bounds().width());
}

auto tab_bar::shift_tabs(float delta) -> void {
  for (auto &c : children()) {
    rect area = c.bounds();
    area.origin.x -= delta;
    c.set_bounds(area);
  }
}

auto tab_bar::set_scroll_offset(float offset) -> bool {
  float const next = std::clamp(snap_to_device(offset, scale()), 0.0f, max_scroll_offset());
  if (next == offset_) {
    return false;
  }
  float const delta = next - offset_;
  offset_ = next;
  if (!needs_layout()) {
    shift_tabs(delta);
  }
  invalidate_paint();
  return true;
}

auto tab_bar::scroll_tab_into_view(std::size_t index) -> void {
  if (index >= tab_count()) {
    return;
  }
  if (needs_layout()) {
    reveal_selected_ = true;
    return;
  }
  // Bounds are in view coordinates, so the tab's place in the row is its bounds plus the offset.
  rect const area = tab_at(index).bounds();
  set_scroll_offset(revealed(area.left() + offset_, area.right() + offset_, offset_,
                             bounds().width(), resolve(fixed(reveal_margin))));
}

auto tab_bar::on_layout() -> void {
  box::on_layout();
  // The box has just placed the tabs unscrolled.
  float width = 0;
  for (auto const &c : children()) {
    if (c.visible()) {
      width = std::max(width, c.bounds().right());
    }
  }
  content_width_ = width;
  float wanted = offset_;
  if (reveal_selected_) {
    reveal_selected_ = false;
    if (auto const index = selected_index()) {
      rect const area = tab_at(*index).bounds();
      wanted = revealed(area.left(), area.right(), wanted, bounds().width(),
                        resolve(fixed(reveal_margin)));
    }
  }
  offset_ = std::clamp(snap_to_device(wanted, scale()), 0.0f, max_scroll_offset());
  if (offset_ != 0) {
    shift_tabs(offset_);
  }
}

auto tab_bar::on_event(event_context &, event const &e) -> bool {
  if (auto const *const w = std::get_if<wheel_event>(&e)) {
    float delta = w->delta.x != 0 ? w->delta.x : w->delta.y;
    if (std::abs(w->delta.y) > std::abs(w->delta.x) && !has(w->modifiers, modifier::shift)) {
      delta = w->delta.y;
    }
    if (!w->precise) {
      delta *= resolve(fixed(wheel_step));
    }
    return set_scroll_offset(offset_ + delta);
  }
  auto const *const k = std::get_if<key_event>(&e);
  if (k == nullptr || !k->pressed || k->modifiers != modifier::none || tree() == nullptr) {
    return false;
  }
  focus_manager &focus = tree()->focus();
  switch (k->code) {
  case key::right:
    return focus.move_focus_in_group(focus_move::next);
  case key::left:
    return focus.move_focus_in_group(focus_move::previous);
  case key::home:
    return focus.move_focus_in_group(focus_move::first);
  case key::end:
    return focus.move_focus_in_group(focus_move::last);
  default:
    return false;
  }
}

auto tab_bar::on_measure(constraints const &limits) const -> basalt::size {
  auto measured = box::on_measure(limits);
  measured.height = std::max(measured.height, resolve(theme_of(*this).sizes.dimension_height_tab));
  return measured;
}

auto tab_bar::on_paint(render_context &context) const -> void {
  if (tree() == nullptr) {
    return;
  }
  float const height = device_stroke_width(fixed(1), scale());
  context.fill_rect({{0, bounds().height() - height}, {bounds().width(), height}},
                    theme_of(*this).colors.border_subdued);
}

} // namespace basalt
