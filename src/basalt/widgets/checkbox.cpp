#include "basalt/widgets/checkbox.h"

#include "basalt/widget_tree.h"

#include <algorithm>
#include <ranges>
#include <utility>

#include "glyphs.h"
#include "named_group.h"
#include "notify.h"

namespace basalt {

namespace a11y = accessibility;

checkbox::checkbox(std::string label, a11y::check_state state, control_size size)
    : choice_control(std::move(label), size), state_(state) {}

auto checkbox::set_state(a11y::check_state state) -> void {
  if (state != state_) {
    state_ = state;
    invalidate_paint();
  }
}

auto checkbox::perform_activation() -> void {
  set_state(state_ == a11y::check_state::checked ? a11y::check_state::unchecked
                                                 : a11y::check_state::checked);
  notify(on_change_, state_);
}

auto checkbox::indicator_size(basalt::theme const &t) const -> basalt::size {
  float const side = resolve(t.sizes.dimension_checkbox_box);
  return {side, side};
}

auto checkbox::paint_indicator(render_context &context, rect box, basalt::theme const &t,
                               control_state state) const -> void {
  auto const &c = t.colors;
  float const radius = resolve(t.sizes.radius_default) / 2;
  float const width = device_stroke_width(fixed(1), scale());
  if (state_ == a11y::check_state::unchecked) {
    rgba const fill = by_state(state, c.background_default, c.background_accent_transparent_hover,
                               c.background_accent_transparent_active);
    context.fill_rounded_rect(box, radius, fill);
    context.stroke_rounded_rect(inset(box, insets::uniform(width / 2)), radius,
                                has_error() ? c.border_danger : c.border_default, width);
    return;
  }
  rgba const fill = has_error() ? by_state(state, c.background_danger, c.background_danger_hover,
                                           c.background_danger_active)
                                : by_state(state, c.background_accent, c.background_accent_hover,
                                           c.background_accent_active);
  context.fill_rounded_rect(box, radius, fill);
  float const mark = device_stroke_width(fixed(2), scale());
  if (state_ == a11y::check_state::checked) {
    glyphs::draw_check(context, box, c.icon_default_on_dark, mark);
  } else {
    glyphs::draw_dash(context, box, c.icon_default_on_dark, mark);
  }
}

auto checkbox::describe(a11y::info &details) const -> void {
  details.kind = a11y::role::check_box;
  details.checked = state_;
  details.actions.insert(a11y::action::press);
}

checkbox_group::checkbox_group(std::string name) : box(axis::vertical), name_(std::move(name)) {
  set_cross_alignment(alignment::start);
  set_focus_scope(focus_scope::none);
}

auto checkbox_group::enable_select_all(std::string label) -> void {
  if (select_all_ != nullptr) {
    select_all_->set_label(std::move(label));
    return;
  }
  select_all_ = &insert_child(0, std::make_unique<checkbox>(std::move(label)));
  select_all_->set_on_change(
      [this](a11y::check_state state) { set_all(state == a11y::check_state::checked); });
}

auto checkbox_group::add_option(std::string label, bool checked) -> checkbox & {
  auto &added = emplace_child<checkbox>(std::move(label), checked ? a11y::check_state::checked
                                                                  : a11y::check_state::unchecked);
  added.set_on_change([this, &added](a11y::check_state) {
    refresh_select_all();
    auto const index = index_of(added);
    notify(on_change_, index ? *index - first_option_child() : 0);
  });
  refresh_select_all();
  return added;
}

auto checkbox_group::option_count() const -> std::size_t {
  return child_count() - first_option_child();
}

auto checkbox_group::option(std::size_t index) const -> checkbox & {
  return static_cast<checkbox &>(child(first_option_child() + index));
}

auto checkbox_group::checked_count() const -> std::size_t {
  return static_cast<std::size_t>(
      std::ranges::count_if(std::views::iota(std::size_t{0}, option_count()),
                            [this](std::size_t i) { return option(i).checked(); }));
}

auto checkbox_group::refresh_select_all() -> void {
  if (select_all_ == nullptr) {
    return;
  }
  std::size_t const checked = checked_count();
  select_all_->set_state(checked == 0                ? a11y::check_state::unchecked
                         : checked == option_count() ? a11y::check_state::checked
                                                     : a11y::check_state::mixed);
}

auto checkbox_group::set_all(bool checked) -> void {
  auto const state = checked ? a11y::check_state::checked : a11y::check_state::unchecked;
  for (std::size_t i = 0; i < option_count(); ++i) {
    option(i).set_state(state);
  }
  if (select_all_ != nullptr) {
    select_all_->set_state(option_count() == 0 ? a11y::check_state::unchecked : state);
  }
}

auto checkbox_group::accessibility_info() const -> a11y::info { return named_group_info(name_); }

} // namespace basalt
