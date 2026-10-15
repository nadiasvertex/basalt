#include "basalt/widgets/menu.h"

#include "basalt/event_router.h"
#include "basalt/widget_tree.h"
#include "basalt/widgets/separator.h"

#include <algorithm>
#include <utility>
#include <variant>

#include "notify.h"

namespace basalt {

namespace {

// Gap between the label and the shortcut hint.
constexpr length hint_gap = fixed(24);

auto check_column(widget const &w) -> float { return w.resolve(w.theme().sizes.dimension_icon); }

} // namespace

menu_item::menu_item(std::string id, std::string label, std::string shortcut,
                     std::optional<bool> checked)
    : id_(std::move(id)), label_(std::move(label), text_role::body),
      shortcut_(std::move(shortcut), text_role::small), checked_(checked) {
  set_focus_policy(focus_policy::strong);
  set_minimum_target(true);
}

auto menu_item::set_label(std::string label) -> void {
  if (label_.set_text(std::move(label))) {
    invalidate_content();
  }
}

auto menu_item::set_shortcut(std::string shortcut) -> void {
  if (shortcut_.set_text(std::move(shortcut))) {
    invalidate_content();
  }
}

auto menu_item::set_checked(std::optional<bool> checked) -> void {
  if (checked != checked_) {
    checked_ = checked;
    invalidate_content();
  }
}

auto menu_item::activate() -> bool {
  if (!enabled_in_tree()) {
    return false;
  }
  // Handlers may remove the menu, so nothing is touched afterwards.
  auto const own = on_activate_;
  if (own) {
    own();
  }
  return true;
}

auto menu_item::accessibility_info() const -> accessibility::info {
  accessibility::info details;
  details.kind = checked_ ? accessibility::role::menu_item_check : accessibility::role::menu_item;
  details.name = label_.text();
  details.keyboard_shortcut = shortcut_.text();
  if (checked_) {
    details.checked =
        *checked_ ? accessibility::check_state::checked : accessibility::check_state::unchecked;
  }
  details.actions = {accessibility::action::press, accessibility::action::focus};
  if (widget const *const p = parent()) {
    std::uint32_t position = 0;
    std::uint32_t count = 0;
    for (widget const &sibling : p->children()) {
      if (dynamic_cast<menu_item const *>(&sibling) != nullptr) {
        ++count;
        if (&sibling == this) {
          position = count;
        }
      }
    }
    details.position_in_set = position;
    details.set_size = count;
  }
  return details;
}

auto menu_item::on_accessibility_action(accessibility::action_request const &request) -> bool {
  return request.kind == accessibility::action::press && activate();
}

auto menu_item::on_event(event_context &, event const &e) -> bool {
  if (auto const *p = std::get_if<pointer_event>(&e)) {
    if (p->button != pointer_button::primary) {
      return false;
    }
    if (p->action == pointer_action::down) {
      return true;
    }
    if (p->action == pointer_action::up) {
      if (interaction().pressed) {
        activate();
      }
      return true;
    }
    return false;
  }
  if (auto const *k = std::get_if<key_event>(&e)) {
    if (k->pressed && !k->repeat && k->modifiers == modifier::none &&
        (k->code == key::enter || k->code == key::space)) {
      activate();
      return true;
    }
  }
  return false;
}

auto menu_item::on_measure(constraints const &) const -> basalt::size {
  auto const &sizes = theme().sizes;
  basalt::size const text = label_.measure(*this);
  basalt::size const hint = shortcut_.measure(*this);
  float const gap = hint.width > 0 ? resolve(hint_gap) : 0;
  float const width = 2 * resolve(sizes.inset_horizontal_menu_item) + check_column(*this) +
                      text.width + gap + hint.width;
  return {width, std::max(text.height + 2 * resolve(sizes.inset_vertical_list_item_text),
                          resolve(sizes.dimension_height_element_min))};
}

auto menu_item::on_paint(render_context &context) const -> void {
  if (tree() == nullptr) {
    return;
  }
  basalt::theme const &t = theme();
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
  float const inset_x = resolve(t.sizes.inset_horizontal_menu_item);
  float const column = check_column(*this);
  if (checked_.value_or(false)) {
    // A check mark in the leading column.
    float const unit = column / 4;
    point const origin{inset_x + unit, area.height() / 2};
    float const width = device_stroke_width(fixed(2), scale());
    context.line(origin, origin + point{unit * 0.9f, unit * 0.9f}, t.colors.icon_default, width);
    context.line(origin + point{unit * 0.9f, unit * 0.9f},
                 origin + point{unit * 2.2f, -unit * 1.2f}, t.colors.icon_default, width);
  }
  rect const text_area{{inset_x + column, 0}, {area.width() - 2 * inset_x - column, area.height()}};
  label_.draw_in(context, *this, text_area, t.colors.text_default);
  shortcut_.draw_in(context, *this, text_area, t.colors.text_subdued, alignment::end);
}

menu::menu(std::string name) : box(axis::vertical), name_(std::move(name)) {
  set_focus_scope(focus_scope::group);
  set_padding(length_insets::symmetric(fixed(0), fixed(4)));
}

auto menu::add_item(std::string id, std::string label, std::string shortcut,
                    std::optional<bool> checked) -> menu_item & {
  auto &item =
      emplace_child<menu_item>(std::move(id), std::move(label), std::move(shortcut), checked);
  item.set_on_activate([this, &item] { item_activated(item); });
  return item;
}

auto menu::add_separator() -> void { emplace_child<separator>(axis::horizontal); }

auto menu::find_item(std::string const &id) const -> menu_item * {
  for (widget &child : children()) {
    if (auto *const item = dynamic_cast<menu_item *>(&child);
        item != nullptr && item->item_id() == id) {
      return item;
    }
  }
  return nullptr;
}

auto menu::item_activated(menu_item const &item) -> void {
  // The handler may remove the menu, so it gets a copy of the id.
  auto const id = item.item_id();
  notify(on_activate_, id);
}

auto menu::accessibility_info() const -> accessibility::info {
  accessibility::info details;
  details.kind = accessibility::role::menu;
  details.name = name_;
  details.orientation = accessibility::orientation::vertical;
  return details;
}

auto menu::on_event(event_context &, event const &e) -> bool {
  auto const *const k = std::get_if<key_event>(&e);
  if (k == nullptr || !k->pressed || tree() == nullptr) {
    return false;
  }
  focus_manager &focus = tree()->focus();
  if (k->modifiers == modifier::none) {
    switch (k->code) {
    case key::down:
      return focus.move_focus_in_group(focus_move::next);
    case key::up:
      return focus.move_focus_in_group(focus_move::previous);
    case key::home:
      return focus.move_focus_in_group(focus_move::first);
    case key::end:
      return focus.move_focus_in_group(focus_move::last);
    case key::escape:
      notify(on_dismiss_);
      return true;
    default:
      break;
    }
  }
  return false;
}

auto menu::on_paint(render_context &context) const -> void {
  if (tree() == nullptr) {
    return;
  }
  basalt::theme const &t = theme();
  rect const area{{}, bounds().extent};
  float const radius = resolve(t.sizes.radius_default);
  float const width = device_stroke_width(fixed(1), scale());
  context.fill_rounded_rect(area, radius, t.colors.background_default);
  context.stroke_rounded_rect(inset(area, insets::uniform(width / 2)), radius,
                              t.colors.border_default, width);
}

} // namespace basalt
