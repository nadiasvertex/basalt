#include "basalt/widgets/button.h"

#include "basalt/event_router.h"
#include "basalt/widget_tree.h"

#include <algorithm>
#include <utility>
#include <variant>

namespace basalt {

namespace {

// `rgba` defaults to opaque black, so "nothing is drawn" has to be spelled out.
constexpr rgba transparent{0, 0, 0, 0};

struct button_colors {
  rgba fill;
  rgba text;
  rgba border;
};

auto colors_for_button(theme const &t, button_variant variant, control_state state)
    -> button_colors {
  auto const &c = t.colors;
  bool const hovered = state == control_state::hovered;
  bool const pressed = state == control_state::pressed;
  switch (variant) {
  case button_variant::primary:
    return {pressed   ? c.background_accent_active
            : hovered ? c.background_accent_hover
                      : c.background_accent,
            c.text_default_on_dark, transparent};
  case button_variant::danger:
    return {pressed   ? c.background_danger_active
            : hovered ? c.background_danger_hover
                      : c.background_danger,
            c.text_default_on_dark, transparent};
  case button_variant::secondary:
    return {pressed   ? c.background_accent_transparent_active
            : hovered ? c.background_accent_transparent_hover
                      : transparent,
            c.text_interactive, c.border_accent};
  case button_variant::tertiary:
    break;
  }
  return {pressed   ? c.background_accent_transparent_active
          : hovered ? c.background_accent_transparent_hover
                    : transparent,
          c.text_interactive, transparent};
}

} // namespace

button::button(std::string label, std::function<void()> on_press)
    : line_(std::move(label), text_role::body_strong), on_press_(std::move(on_press)) {
  set_focus_policy(focus_policy::strong);
  set_minimum_target(true);
}

auto button::set_label(std::string label) -> void {
  if (line_.set_text(std::move(label))) {
    invalidate_layout();
    invalidate_paint();
  }
}

auto button::set_variant(button_variant variant) -> void {
  if (variant != variant_) {
    variant_ = variant;
    invalidate_paint();
  }
}

auto button::set_label_alignment(alignment a) -> void {
  if (a != alignment_) {
    alignment_ = a;
    invalidate_paint();
  }
}

auto button::set_shortcut(std::string shortcut) -> void { shortcut_ = std::move(shortcut); }

auto button::press() -> bool {
  if (!enabled_in_tree()) {
    return false;
  }
  if (on_press_) {
    // The handler may destroy this button; do not touch members afterwards.
    auto const handler = on_press_;
    handler();
  }
  return true;
}

auto button::accessibility_info() const -> accessibility::info {
  accessibility::info details;
  details.kind = accessibility::role::button;
  details.name = line_.text();
  details.keyboard_shortcut = shortcut_;
  details.actions = {accessibility::action::press, accessibility::action::focus};
  return details;
}

auto button::on_accessibility_action(accessibility::action_request const &request) -> bool {
  return request.kind == accessibility::action::press && press();
}

auto button::on_event(event_context &, event const &e) -> bool {
  if (auto const *p = std::get_if<pointer_event>(&e)) {
    if (p->button != pointer_button::primary) {
      return false;
    }
    if (p->action == pointer_action::down) {
      return true;
    }
    if (p->action == pointer_action::up) {
      if (interaction().pressed) {
        press();
      }
      return true;
    }
    return false;
  }
  if (auto const *k = std::get_if<key_event>(&e)) {
    if (k->pressed && !k->repeat && k->modifiers == modifier::none &&
        (k->code == key::enter || k->code == key::space)) {
      press();
      return true;
    }
  }
  return false;
}

auto button::on_measure(constraints const &) const -> basalt::size {
  widget_tree const *const t = tree();
  basalt::theme const fallback;
  auto const &sizes = (t != nullptr ? t->theme() : fallback).sizes;
  basalt::size const text = line_.measure(*this);
  float const padding = resolve(sizes.inset_horizontal_button) * 2;
  return {std::max(text.width + padding, resolve(sizes.dimension_width_element_min)),
          std::max(text.height, resolve(sizes.dimension_height_element_min))};
}

auto button::on_paint(render_context &context) const -> void {
  if (tree() == nullptr) {
    return;
  }
  theme const &t = tree()->theme();
  auto const colors = colors_for_button(t, variant_, control_state_of(interaction(), enabled()));
  rect const area{{}, bounds().extent};
  float const radius = resolve(t.sizes.radius_default);
  if (colors.fill.a != 0) {
    context.fill_rounded_rect(area, radius, colors.fill);
  }
  if (colors.border.a != 0) {
    float const width = device_stroke_width(fixed(1), scale());
    context.stroke_rounded_rect(inset(area, insets::uniform(width / 2)), radius, colors.border,
                                width);
  }
  float const edge = resolve(t.sizes.inset_horizontal_button);
  line_.draw_in(context, *this,
                alignment_ == alignment::center ? area : inset(area, insets::symmetric(edge, 0)),
                colors.text, alignment_, alignment::center);
}

} // namespace basalt
