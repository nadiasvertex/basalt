#pragma once

#include "basalt/widget.h"
#include "basalt/widgets/control_size.h"
#include "basalt/widgets/text_line.h"

#include <string>

namespace basalt {

// The shared anatomy of toggles, checkboxes and radio buttons: an indicator
// at the start, a label beside it, optional helper text below the label and
// optional trailing text. The control is a Tab stop. A primary press released
// over it or Space activates it. Disabled controls stay focusable and do
// nothing. Derived classes supply the indicator, what activation does and
// what the control reports to assistive technology.
class choice_control : public widget {
public:
  [[nodiscard]] auto label() const -> std::string const & { return label_.text(); }
  auto set_label(std::string label) -> void;
  [[nodiscard]] auto helper_text() const -> std::string const & { return helper_.text(); }
  // Shown below the label. In the error state it holds the error message.
  auto set_helper_text(std::string text) -> void;
  [[nodiscard]] auto has_error() const -> bool { return error_; }
  auto set_error(bool error) -> void;
  [[nodiscard]] auto size() const -> control_size { return size_; }
  auto set_size(control_size size) -> void;

  // Does what a click does. False when the control is disabled.
  auto activate() -> bool;

  [[nodiscard]] auto accessibility_info() const -> accessibility::info override final;

protected:
  choice_control(std::string label, control_size size);

  // Text after the label, such as a toggle's status.
  auto set_trailing_text(std::string text) -> void;
  [[nodiscard]] auto trailing_text() const -> std::string const & { return trailing_.text(); }

  // The user activated the control.
  virtual auto perform_activation() -> void = 0;
  // Enter also activates when true; Space always does.
  [[nodiscard]] virtual auto activates_on_enter() const -> bool { return false; }
  // The indicator's size in layout pixels.
  [[nodiscard]] virtual auto indicator_size(basalt::theme const &t) const -> basalt::size = 0;
  // Draws the indicator inside `box`, which has `indicator_size`.
  virtual auto paint_indicator(render_context &context, rect box, basalt::theme const &t,
                               control_state state) const -> void = 0;
  // Sets the role, state and actions; the name and description are added.
  virtual auto describe(accessibility::info &details) const -> void = 0;
  auto on_event(event_context &context, event const &e) -> bool override;
  auto on_paint(render_context &context) const -> void override;
  [[nodiscard]] auto on_measure(constraints const &limits) const -> basalt::size override;
  auto on_accessibility_action(accessibility::action_request const &request) -> bool override;

private:
  struct metrics;
  [[nodiscard]] auto measure_parts() const -> metrics;

  text_line label_;
  text_line helper_;
  text_line trailing_;
  control_size size_;
  bool error_ = false;
};

} // namespace basalt
