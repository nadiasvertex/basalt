#include "basalt/widgets/button.h"

#include "basalt/event_router.h"
#include "basalt/widget_tree.h"

#include <algorithm>
#include <utility>
#include <variant>

#include "notify.h"
#include "press_activation.h"

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
  switch (variant) {
  case button_variant::primary:
    return {
        by_state(state, c.background_accent, c.background_accent_hover, c.background_accent_active),
        c.text_default_on_dark, transparent};
  case button_variant::danger:
    return {
        by_state(state, c.background_danger, c.background_danger_hover, c.background_danger_active),
        c.text_default_on_dark, transparent};
  case button_variant::secondary:
    return {by_state(state, transparent, c.background_accent_transparent_hover,
                     c.background_accent_transparent_active),
            c.text_interactive, c.border_accent};
  case button_variant::tertiary:
    break;
  }
  return {by_state(state, transparent, c.background_accent_transparent_hover,
                   c.background_accent_transparent_active),
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
    invalidate_content();
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
  notify(on_press_);
  return true;
}

auto button::accessibility_info() const -> accessibility::info {
  accessibility::info details;
  details.kind = accessibility::role::button;
  details.name = accessible_name_.empty() ? line_.text() : accessible_name_;
  if (toggled_) {
    details.checked = *toggled_ ? accessibility::check_state::checked : accessibility::check_state::unchecked;
  }
  details.keyboard_shortcut = shortcut_;
  details.actions = {accessibility::action::press, accessibility::action::focus};
  return details;
}

auto button::on_accessibility_action(accessibility::action_request const &request) -> bool {
  return request.kind == accessibility::action::press && press();
}

auto button::on_event(event_context &, event const &e) -> bool {
  return handle_press_activation(e, interaction(), {}, [this] { press(); });
}

auto button::on_measure(constraints const &) const -> basalt::size {
  auto const &sizes = theme().sizes;
  basalt::size const text = line_.measure(*this);
  float const padding = resolve(sizes.inset_horizontal_button) * 2;
  return {std::max(text.width + padding, resolve(sizes.dimension_width_element_min)),
          std::max(text.height, resolve(sizes.dimension_height_element_min))};
}

auto button::on_paint(render_context &context) const -> void {
  if (tree() == nullptr) {
    return;
  }
  basalt::theme const &t = theme();
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
