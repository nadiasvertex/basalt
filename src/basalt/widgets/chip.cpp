#include "basalt/widgets/chip.h"

#include "basalt/event_router.h"
#include "basalt/render_context.h"
#include "basalt/widget_tree.h"

#include <algorithm>
#include <utility>
#include <variant>

#include "glyphs.h"
#include "notify.h"

namespace basalt {

namespace a11y = accessibility;

namespace {

constexpr float vertical_padding = 4;
constexpr float remove_mark_side = 16;

} // namespace

struct chip::layout_parts {
  float inset = 0;
  float gap = 0;
  basalt::size label;
  // The label width after the maximum width is applied.
  float label_width = 0;
  float mark = 0;
  float width = 0;
  float height = 0;
  bool truncated = false;
};

chip::chip(std::string label, control_size size)
    : line_(std::move(label), label_role_for(size)), size_(size) {
  set_focus_policy(focus_policy::strong);
  set_minimum_target(true);
}

auto chip::parts() const -> layout_parts {
  basalt::theme const &t = theme();
  layout_parts p;
  p.inset = resolve(t.sizes.inset_horizontal_chip_text);
  p.gap = resolve(t.sizes.gap_horizontal_shape_text);
  p.label = line_.measure(*this);
  p.mark = removable_ ? resolve(fixed(remove_mark_side)) : 0;
  float const fixed_width = 2 * p.inset + (removable_ ? p.gap + p.mark : 0);
  float const natural = fixed_width + p.label.width;
  p.width = natural;
  if (max_width_) {
    float const limit = std::max(resolve(*max_width_), fixed_width);
    p.width = std::min(natural, limit);
  }
  p.label_width = std::max(0.0f, p.width - fixed_width);
  p.truncated = p.label_width < p.label.width;
  p.height = p.label.height + 2 * resolve(fixed(vertical_padding));
  return p;
}

auto chip::set_label(std::string label) -> void {
  if (line_.set_text(std::move(label))) {
    refresh_tooltip();
    invalidate_content();
  }
}

auto chip::set_size(control_size size) -> void {
  if (size != size_) {
    size_ = size;
    line_.set_role(label_role_for(size));
    invalidate_content();
  }
}

auto chip::set_selectable(bool selectable) -> void {
  if (selectable != selectable_) {
    selectable_ = selectable;
    invalidate_paint();
  }
}

auto chip::set_selected(bool selected) -> void {
  if (selected != selected_) {
    selected_ = selected;
    invalidate_paint();
  }
}

auto chip::set_removable(bool removable) -> void {
  if (removable != removable_) {
    removable_ = removable;
    invalidate_content();
    if (auto *const group = parent_as<chip_group>()) {
      group->refresh_remove_all();
    }
  }
}

auto chip::set_error(bool error) -> void {
  if (error != error_) {
    error_ = error;
    invalidate_paint();
  }
}

auto chip::set_loading(bool loading) -> void {
  if (loading != loading_) {
    loading_ = loading;
    invalidate_paint();
  }
}

auto chip::set_max_width(std::optional<length> width) -> void {
  if (width != max_width_) {
    max_width_ = width;
    invalidate_content();
  }
}

auto chip::can_act() const -> bool { return enabled_in_tree() && !loading_; }

auto chip::toggle() -> bool {
  if (!selectable_ || !can_act()) {
    return false;
  }
  set_selected(!selected_);
  bool const state = selected_;
  if (auto *const group = parent_as<chip_group>()) {
    group->chip_toggled(*this);
  }
  notify(on_toggle_, state);
  return true;
}

auto chip::remove() -> bool {
  if (!removable_ || !can_act()) {
    return false;
  }
  if (on_remove_) {
    auto const handler = on_remove_;
    handler();
  }
  // The handler may have removed the chip itself.
  if (auto *const group = parent_as<chip_group>()) {
    group->chip_removed(*this);
  }
  return true;
}

auto chip::accessibility_info() const -> a11y::info {
  a11y::info details;
  details.kind = selectable_ ? a11y::role::check_box : a11y::role::button;
  details.name = line_.text();
  if (selectable_) {
    details.checked = selected_ ? a11y::check_state::checked : a11y::check_state::unchecked;
    details.selected = selected_;
  }
  details.invalid = error_;
  details.busy = loading_;
  details.actions.insert(a11y::action::focus);
  if (selectable_ || removable_) {
    details.actions.insert(a11y::action::press);
  }
  return details;
}

auto chip::on_accessibility_action(a11y::action_request const &request) -> bool {
  if (request.kind != a11y::action::press) {
    return false;
  }
  return selectable_ ? toggle() : remove();
}

auto chip::on_event(event_context &, event const &e) -> bool {
  if (auto const *p = std::get_if<pointer_event>(&e)) {
    if (p->button != pointer_button::primary) {
      return false;
    }
    if (p->action == pointer_action::down) {
      return true;
    }
    if (p->action == pointer_action::up) {
      if (interaction().pressed) {
        layout_parts const m = parts();
        bool const on_mark = removable_ && p->position.x >= m.width - m.inset - m.mark;
        if (on_mark || !selectable_) {
          remove();
        } else {
          toggle();
        }
      }
      return true;
    }
    return false;
  }
  if (auto const *k = std::get_if<key_event>(&e);
      k != nullptr && k->pressed && !k->repeat && k->modifiers == modifier::none) {
    switch (k->code) {
    case key::space:
    case key::enter:
      if (selectable_) {
        toggle();
      } else {
        remove();
      }
      return true;
    case key::backspace:
    case key::delete_forward:
      return remove() || removable_;
    default:
      break;
    }
  }
  return false;
}

auto chip::on_measure(constraints const &) const -> basalt::size {
  layout_parts const m = parts();
  return {m.width, m.height};
}

auto chip::refresh_tooltip() -> void {
  tooltip_.set_text(*this, parts().truncated ? line_.text() : std::string{});
}

auto chip::on_interaction_changed(interaction_state const &previous) -> void {
  widget::on_interaction_changed(previous);
  refresh_tooltip();
  if (tooltip_.update(*this)) {
    request_animation_frame();
  }
}

auto chip::on_animation_frame(std::chrono::nanoseconds elapsed) -> bool {
  return tooltip_.advance(*this, elapsed);
}

auto chip::on_detached() -> void { tooltip_.hide(*this); }

auto chip::on_paint(render_context &context) const -> void {
  if (tree() == nullptr) {
    return;
  }
  basalt::theme const &t = theme();
  auto const &c = t.colors;
  layout_parts const m = parts();
  rect const area{{}, {m.width, m.height}};
  float const radius = std::min(resolve(t.sizes.radius_chip), m.height / 2);
  control_state const state = control_state_of(interaction(), enabled());

  rgba fill = by_state(state, c.background_secondary, c.background_neutral_hover,
                       c.background_neutral_active);
  if (selected_) {
    fill = by_state(state, c.background_accent_subdued, c.background_accent_subdued_hover,
                    c.background_accent_subdued_active);
  }
  context.fill_rounded_rect(area, radius, fill);
  float const width = device_stroke_width(fixed(1), scale());
  if (error_) {
    context.stroke_rounded_rect(inset(area, insets::uniform(width / 2)), radius, c.border_danger,
                                width);
  } else if (selected_) {
    context.stroke_rounded_rect(inset(area, insets::uniform(width / 2)), radius, c.border_accent,
                                width);
  }

  rgba const text = state == control_state::disabled || loading_ ? c.text_subdued
                    : error_                                     ? c.text_danger
                                                                 : c.text_default;
  rect const label_area{{m.inset, 0}, {m.label_width, m.height}};
  line_.draw_in(context, *this, label_area, text, alignment::start, alignment::center);
  if (removable_ && !loading_) {
    rect const mark{{m.width - m.inset - m.mark, (m.height - m.mark) / 2}, {m.mark, m.mark}};
    glyphs::draw_close(context, mark, error_ ? c.icon_danger : c.icon_default, width);
  }
}

// ----------------------------------------------------------------------------

chip_group::chip_group(std::string name, chip_selection selection)
    : name_(std::move(name)), selection_(selection) {
  set_focus_scope(focus_scope::group);
}

auto chip_group::add_chip(std::string label) -> chip & {
  auto &added = emplace_child<chip>(std::move(label));
  added.set_selectable(selection_ != chip_selection::none);
  // Keep the Remove All button last.
  if (auto *const all = remove_all_button()) {
    move_child(*all, child_count() - 1);
  }
  return added;
}

auto chip_group::remove_all_button() const -> button * {
  for (widget &c : children()) {
    if (c.id() == remove_all_id_) {
      return static_cast<button *>(&c);
    }
  }
  return nullptr;
}

auto chip_group::chip_count() const -> std::size_t {
  return child_count() - (remove_all_button() != nullptr ? 1 : 0);
}

auto chip_group::chip_at(std::size_t index) const -> chip & {
  return static_cast<chip &>(child(index));
}

auto chip_group::selected_indices() const -> std::vector<std::size_t> {
  std::vector<std::size_t> out;
  for (std::size_t i = 0; i < chip_count(); ++i) {
    if (chip_at(i).selected()) {
      out.push_back(i);
    }
  }
  return out;
}

auto chip_group::set_selection(chip_selection selection) -> void {
  selection_ = selection;
  for (std::size_t i = 0; i < chip_count(); ++i) {
    chip_at(i).set_selectable(selection != chip_selection::none);
    if (selection == chip_selection::none) {
      chip_at(i).set_selected(false);
    }
  }
}

auto chip_group::set_remove_all_label(std::string label) -> void {
  remove_all_label_ = std::move(label);
  refresh_remove_all();
}

auto chip_group::refresh_remove_all() -> void {
  bool any_removable = false;
  for (std::size_t i = 0; i < chip_count(); ++i) {
    any_removable = any_removable || chip_at(i).removable();
  }
  bool const wanted = !remove_all_label_.empty() && any_removable;
  button *const existing = remove_all_button();
  if (wanted && existing == nullptr) {
    auto &added = emplace_child<button>(remove_all_label_, [this] { remove_all(); });
    added.set_variant(button_variant::tertiary);
    remove_all_id_ = added.id();
  } else if (wanted) {
    existing->set_label(remove_all_label_);
  } else if (existing != nullptr) {
    remove_all_id_ = {};
    if (tree() != nullptr) {
      tree()->destroy_later(*existing);
    } else {
      (void)remove_child(*existing);
    }
  }
}

auto chip_group::remove_all() -> void {
  std::vector<chip *> doomed;
  for (std::size_t i = 0; i < chip_count(); ++i) {
    if (chip_at(i).removable()) {
      doomed.push_back(&chip_at(i));
    }
  }
  for (chip *const c : doomed) {
    c->remove();
  }
}

auto chip_group::chip_toggled(chip &source) -> void {
  auto const index = index_of(source);
  if (!index) {
    return;
  }
  if (selection_ == chip_selection::single && source.selected()) {
    for (std::size_t i = 0; i < chip_count(); ++i) {
      if (i != *index) {
        chip_at(i).set_selected(false);
      }
    }
  }
  if (on_change_) {
    auto const handler = on_change_;
    handler(*index);
  }
}

auto chip_group::chip_removed(chip &source) -> void {
  if (tree() != nullptr) {
    (void)tree()->destroy_later(source);
  } else {
    (void)remove_child(source);
  }
}

auto chip_group::on_child_removed(widget &child) -> void {
  if (child.id() == remove_all_id_) {
    remove_all_id_ = {};
  }
  refresh_remove_all();
}

auto chip_group::flow(float max_width, bool place) const -> basalt::size {
  basalt::theme const &t = theme();
  float const gap = resolve(t.sizes.gap_horizontal_shape_text);
  float x = 0;
  float y = 0;
  float row_height = 0;
  float widest = 0;
  for (widget &c : children()) {
    if (!c.visible()) {
      continue;
    }
    basalt::size const wanted = c.measure(constraints::loose({max_width, 1e9f}));
    if (x > 0 && x + wanted.width > max_width) {
      y += row_height + gap;
      x = 0;
      row_height = 0;
    }
    if (place) {
      c.set_bounds({{x, y}, wanted});
    }
    x += wanted.width + gap;
    row_height = std::max(row_height, wanted.height);
    widest = std::max(widest, x - gap);
  }
  return {widest, y + row_height};
}

auto chip_group::on_measure(constraints const &limits) const -> basalt::size {
  return flow(limits.max.width, false);
}

auto chip_group::on_layout() -> void { (void)flow(bounds().width(), true); }

auto chip_group::accessibility_info() const -> a11y::info {
  a11y::info details;
  details.kind = a11y::role::group;
  details.name = name_;
  return details;
}

} // namespace basalt
