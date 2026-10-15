#include "basalt/widgets/choice_control.h"

#include "basalt/event_router.h"
#include "basalt/widget_tree.h"

#include <algorithm>
#include <utility>
#include <variant>

#include "press_activation.h"

namespace basalt {

struct choice_control::metrics {
  basalt::size indicator;
  basalt::size label;
  basalt::size trailing;
  basalt::size helper;
  float gap = 0;
  float helper_gap = 0;
  float row_height = 0;
  bool has_helper = false;
};

choice_control::choice_control(std::string label, control_size size)
    : field_(std::move(label), size), trailing_({}, label_role_for(size)) {
  set_focus_policy(focus_policy::strong);
  set_minimum_target(true);
}

auto choice_control::measure_parts() const -> metrics {
  basalt::theme const &t = theme();
  metrics m;
  m.indicator = indicator_size(t);
  m.label = field_.label().measure(*this);
  m.trailing = trailing_.text().empty() ? basalt::size{} : trailing_.measure(*this);
  m.has_helper = !field_.helper().text().empty();
  m.helper = m.has_helper ? field_.helper().measure(*this) : basalt::size{};
  m.gap = resolve(t.sizes.gap_horizontal_shape_text);
  m.helper_gap = resolve(t.sizes.gap_vertical_field_helper_text);
  m.row_height = std::max({m.indicator.height, m.label.height, m.trailing.height});
  return m;
}

auto choice_control::set_label(std::string label) -> void {
  if (field_.set_label(std::move(label))) {
    invalidate_content();
  }
}

auto choice_control::set_helper_text(std::string text) -> void {
  if (field_.set_helper(std::move(text))) {
    invalidate_content();
  }
}

auto choice_control::set_trailing_text(std::string text) -> void {
  if (trailing_.set_text(std::move(text))) {
    invalidate_content();
  }
}

auto choice_control::set_error(bool error) -> void {
  if (field_.set_error(error)) {
    invalidate_paint();
  }
}

auto choice_control::set_size(control_size size) -> void {
  if (field_.set_size(size)) {
    trailing_.set_role(label_role_for(size));
    invalidate_content();
  }
}

auto choice_control::activate() -> bool {
  if (!enabled_in_tree()) {
    return false;
  }
  perform_activation();
  return true;
}

auto choice_control::accessibility_info() const -> accessibility::info {
  accessibility::info details;
  describe(details);
  details.name = field_.label().text();
  details.description = field_.helper().text();
  details.invalid = field_.has_error();
  details.actions.insert(accessibility::action::focus);
  return details;
}

auto choice_control::on_accessibility_action(accessibility::action_request const &request) -> bool {
  return request.kind == accessibility::action::press && activate();
}

auto choice_control::on_event(event_context &, event const &e) -> bool {
  return handle_press_activation(e, interaction(), {.enter = activates_on_enter()},
                                 [this] { activate(); });
}

auto choice_control::on_measure(constraints const &) const -> basalt::size {
  metrics const m = measure_parts();
  basalt::theme const &t = theme();
  float const text_start = m.indicator.width + m.gap;
  float width = text_start + m.label.width;
  if (m.trailing.width > 0) {
    width += m.gap + m.trailing.width;
  }
  width = std::max(width, text_start + m.helper.width);
  float height = std::max(m.row_height, resolve(t.sizes.dimension_height_element_min));
  if (m.has_helper) {
    height += m.helper_gap + m.helper.height;
  }
  return {width, height};
}

auto choice_control::on_paint(render_context &context) const -> void {
  if (tree() == nullptr) {
    return;
  }
  basalt::theme const &t = theme();
  metrics const m = measure_parts();
  float const row = std::max(m.row_height, resolve(t.sizes.dimension_height_element_min));
  rect const box{{0, (row - m.indicator.height) / 2}, m.indicator};
  paint_indicator(context, box, t, control_state_of(interaction(), enabled()));

  float const text_x = m.indicator.width + m.gap;
  rect const label_area{{text_x, 0}, {m.label.width, row}};
  field_.label().draw_in(context, *this, label_area, t.colors.text_default, alignment::start,
                         alignment::center);
  if (m.trailing.width > 0) {
    rect const trailing_area{{text_x + m.label.width + m.gap, 0}, {m.trailing.width, row}};
    trailing_.draw_in(context, *this, trailing_area, t.colors.text_subdued, alignment::start,
                      alignment::center);
  }
  if (m.has_helper) {
    field_.helper().draw(context, *this, {text_x, row + m.helper_gap},
                         field_.has_error() ? t.colors.text_danger : t.colors.text_subdued);
  }
}

} // namespace basalt
