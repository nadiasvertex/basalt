#include "basalt/widgets/radio_button.h"

#include "basalt/event_router.h"
#include "basalt/focus.h"
#include "basalt/widget_tree.h"

#include <utility>
#include <variant>

namespace basalt {

namespace a11y = accessibility;

radio_button::radio_button(std::string label, control_size size)
    : choice_control(std::move(label), size) {}

auto radio_button::set_selected(bool selected) -> void {
  if (selected != selected_) {
    selected_ = selected;
    invalidate_paint();
  }
}

auto radio_button::perform_activation() -> void {
  if (auto *const group = dynamic_cast<radio_group *>(parent())) {
    group->choose(*this);
    return;
  }
  set_selected(true);
}

auto radio_button::indicator_size(theme const &t) const -> basalt::size {
  float const side = resolve(t.sizes.dimension_checkbox_box);
  return {side, side};
}

auto radio_button::paint_indicator(render_context &context, rect box, theme const &t,
                                   control_state state) const -> void {
  auto const &c = t.colors;
  bool const hovered = state == control_state::hovered;
  bool const pressed = state == control_state::pressed;
  float const radius = box.width() / 2;
  float const width = device_stroke_width(fixed(1), scale());
  rgba const accent = pressed   ? c.background_accent_active
                      : hovered ? c.background_accent_hover
                                : c.background_accent;
  context.fill_rounded_rect(box, radius,
                            pressed   ? c.background_accent_transparent_active
                            : hovered ? c.background_accent_transparent_hover
                                      : c.background_default);
  context.stroke_rounded_rect(inset(box, insets::uniform(width / 2)), radius,
                              selected_ ? accent : c.border_default, width);
  if (selected_) {
    float const dot = box.width() / 2;
    context.fill_rounded_rect(
        {{box.origin.x + (box.width() - dot) / 2, box.origin.y + (box.height() - dot) / 2},
         {dot, dot}},
        dot / 2, accent);
  }
}

auto radio_button::describe(a11y::info &details) const -> void {
  details.kind = a11y::role::radio_button;
  details.checked = selected_ ? a11y::check_state::checked : a11y::check_state::unchecked;
  details.selected = selected_;
  details.actions.insert(a11y::action::press);
}

auto radio_button::on_event(event_context &context, event const &e) -> bool {
  if (auto const *k = std::get_if<key_event>(&e);
      k != nullptr && k->pressed && k->modifiers == modifier::none && tree() != nullptr &&
      dynamic_cast<radio_group *>(parent()) != nullptr) {
    focus_manager &focus = tree()->focus();
    bool moved = false;
    switch (k->code) {
    case key::down:
    case key::right:
      moved = focus.move_focus_in_group(focus_move::next);
      break;
    case key::up:
    case key::left:
      moved = focus.move_focus_in_group(focus_move::previous);
      break;
    default:
      return choice_control::on_event(context, e);
    }
    if (moved) {
      if (auto *const target = dynamic_cast<radio_button *>(focus.focused())) {
        target->activate();
      }
    }
    return true;
  }
  return choice_control::on_event(context, e);
}

radio_group::radio_group(std::string name, axis direction)
    : box(direction), name_(std::move(name)) {
  set_cross_alignment(alignment::start);
  set_focus_scope(focus_scope::group);
}

auto radio_group::add_option(std::string label) -> radio_button & {
  return emplace_child<radio_button>(std::move(label));
}

auto radio_group::option(std::size_t index) const -> radio_button & {
  return static_cast<radio_button &>(child(index));
}

auto radio_group::selected_index() const -> std::optional<std::size_t> {
  for (std::size_t i = 0; i < option_count(); ++i) {
    if (option(i).selected()) {
      return i;
    }
  }
  return std::nullopt;
}

auto radio_group::select(std::size_t index) -> bool {
  if (index >= option_count()) {
    return false;
  }
  for (std::size_t i = 0; i < option_count(); ++i) {
    option(i).set_selected(i == index);
  }
  return true;
}

auto radio_group::choose(radio_button const &button) -> void {
  auto const index = index_of(button);
  if (!index || selected_index() == index) {
    return;
  }
  select(*index);
  if (on_change_) {
    auto const handler = on_change_;
    handler(*index);
  }
}

auto radio_group::accessibility_info() const -> a11y::info {
  a11y::info details;
  details.kind = a11y::role::group;
  details.name = name_;
  return details;
}

} // namespace basalt
