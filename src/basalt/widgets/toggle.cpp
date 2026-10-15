#include "basalt/widgets/toggle.h"

#include "basalt/widget_tree.h"

#include <utility>

namespace basalt {

toggle::toggle(std::string label, bool on, control_size size)
    : choice_control(std::move(label), size), on_(on) {}

auto toggle::set_on(bool on) -> void {
  if (on == on_) {
    return;
  }
  on_ = on;
  refresh_status();
  invalidate_paint();
}

auto toggle::set_status_text(std::string off_text, std::string on_text) -> void {
  has_status_ = true;
  off_text_ = std::move(off_text);
  on_text_ = std::move(on_text);
  refresh_status();
}

auto toggle::clear_status_text() -> void {
  has_status_ = false;
  set_trailing_text({});
}

auto toggle::refresh_status() -> void {
  if (has_status_) {
    set_trailing_text(on_ ? on_text_ : off_text_);
  }
}

auto toggle::perform_activation() -> void {
  set_on(!on_);
  if (on_change_) {
    // The handler may destroy this toggle; do not touch members afterwards.
    auto const handler = on_change_;
    handler(on_);
  }
}

auto toggle::indicator_size(theme const &t) const -> basalt::size {
  return {resolve(t.sizes.dimension_width_toggle_track),
          resolve(t.sizes.dimension_height_toggle_track)};
}

auto toggle::paint_indicator(render_context &context, rect box, theme const &t,
                             control_state state) const -> void {
  auto const &c = t.colors;
  bool const hovered = state == control_state::hovered;
  bool const pressed = state == control_state::pressed;
  float const radius = box.height() / 2;
  rgba const fill = on_ ? (pressed   ? c.background_accent_active
                           : hovered ? c.background_accent_hover
                                     : c.background_accent)
                        : (pressed   ? c.background_neutral_active
                           : hovered ? c.background_neutral_hover
                                     : c.background_secondary);
  context.fill_rounded_rect(box, radius, fill);
  if (!on_) {
    float const width = device_stroke_width(fixed(1), scale());
    context.stroke_rounded_rect(inset(box, insets::uniform(width / 2)), radius, c.border_default,
                                width);
  }
  float const edge = resolve(t.sizes.inset_toggle_track);
  float const handle = resolve(t.sizes.dimension_toggle_handle);
  float const x = on_ ? box.width() - edge - handle : edge;
  rect const handle_box{{box.origin.x + x, box.origin.y + (box.height() - handle) / 2},
                        {handle, handle}};
  context.fill_rounded_rect(handle_box, handle / 2,
                            on_ ? c.icon_default_on_dark : c.icon_neutral_default);
}

auto toggle::describe(accessibility::info &details) const -> void {
  details.kind = accessibility::role::switch_control;
  details.checked =
      on_ ? accessibility::check_state::checked : accessibility::check_state::unchecked;
  details.value = on_ ? on_text_ : off_text_;
  details.actions.insert(accessibility::action::press);
}

} // namespace basalt
