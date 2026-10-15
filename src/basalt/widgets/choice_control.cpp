#include "basalt/widgets/choice_control.h"

#include "basalt/event_router.h"
#include "basalt/widget_tree.h"

#include <algorithm>
#include <utility>
#include <variant>

namespace basalt {

namespace {

auto theme_of(widget const &w, theme const &fallback) -> theme const & {
  return w.tree() != nullptr ? w.tree()->theme() : fallback;
}

constexpr text_role helper_role = text_role::small;

} // namespace

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
    : label_(std::move(label), label_role_for(size)), helper_({}, helper_role),
      trailing_({}, label_role_for(size)), size_(size) {
  set_focus_policy(focus_policy::strong);
  set_minimum_target(true);
}

auto choice_control::measure_parts() const -> metrics {
  basalt::theme const fallback;
  theme const &t = theme_of(*this, fallback);
  metrics m;
  m.indicator = indicator_size(t);
  m.label = label_.measure(*this);
  m.trailing = trailing_.text().empty() ? basalt::size{} : trailing_.measure(*this);
  m.has_helper = !helper_.text().empty();
  m.helper = m.has_helper ? helper_.measure(*this) : basalt::size{};
  m.gap = resolve(t.sizes.gap_horizontal_shape_text);
  m.helper_gap = resolve(t.sizes.gap_vertical_field_helper_text);
  m.row_height = std::max({m.indicator.height, m.label.height, m.trailing.height});
  return m;
}

auto choice_control::set_label(std::string label) -> void {
  if (label_.set_text(std::move(label))) {
    invalidate_layout();
    invalidate_paint();
  }
}

auto choice_control::set_helper_text(std::string text) -> void {
  if (helper_.set_text(std::move(text))) {
    invalidate_layout();
    invalidate_paint();
  }
}

auto choice_control::set_trailing_text(std::string text) -> void {
  if (trailing_.set_text(std::move(text))) {
    invalidate_layout();
    invalidate_paint();
  }
}

auto choice_control::set_error(bool error) -> void {
  if (error != error_) {
    error_ = error;
    invalidate_paint();
  }
}

auto choice_control::set_size(control_size size) -> void {
  if (size == size_) {
    return;
  }
  size_ = size;
  label_.set_role(label_role_for(size));
  trailing_.set_role(label_role_for(size));
  invalidate_layout();
  invalidate_paint();
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
  details.name = label_.text();
  details.description = helper_.text();
  details.invalid = error_;
  details.actions.insert(accessibility::action::focus);
  return details;
}

auto choice_control::on_accessibility_action(accessibility::action_request const &request) -> bool {
  return request.kind == accessibility::action::press && activate();
}

auto choice_control::on_event(event_context &, event const &e) -> bool {
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
        (k->code == key::space || (k->code == key::enter && activates_on_enter()))) {
      activate();
      return true;
    }
  }
  return false;
}

auto choice_control::on_measure(constraints const &) const -> basalt::size {
  metrics const m = measure_parts();
  basalt::theme const fallback;
  theme const &t = theme_of(*this, fallback);
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
  theme const &t = tree()->theme();
  metrics const m = measure_parts();
  float const row = std::max(m.row_height, resolve(t.sizes.dimension_height_element_min));
  rect const box{{0, (row - m.indicator.height) / 2}, m.indicator};
  paint_indicator(context, box, t, control_state_of(interaction(), enabled()));

  float const text_x = m.indicator.width + m.gap;
  rect const label_area{{text_x, 0}, {m.label.width, row}};
  label_.draw_in(context, *this, label_area, t.colors.text_default, alignment::start,
                 alignment::center);
  if (m.trailing.width > 0) {
    rect const trailing_area{{text_x + m.label.width + m.gap, 0}, {m.trailing.width, row}};
    trailing_.draw_in(context, *this, trailing_area, t.colors.text_subdued, alignment::start,
                      alignment::center);
  }
  if (m.has_helper) {
    helper_.draw(context, *this, {text_x, row + m.helper_gap},
                 error_ ? t.colors.text_danger : t.colors.text_subdued);
  }
}

} // namespace basalt
