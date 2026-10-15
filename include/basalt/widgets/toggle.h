#pragma once

#include "basalt/widgets/choice_control.h"

#include <functional>
#include <string>

namespace basalt {

// An on/off switch whose effect is immediate, like a light switch. It has no
// error or indeterminate state. Optional status text after the label says
// what the current value is, such as "Off" and "On".
class toggle : public choice_control {
public:
  explicit toggle(std::string label = {}, bool on = false,
                  control_size size = control_size::medium);

  [[nodiscard]] auto on() const -> bool { return on_; }
  // Does not call the change handler.
  auto set_on(bool on) -> void;
  // Called with the new value after the user changes it.
  auto set_on_change(std::function<void(bool)> handler) -> void { on_change_ = std::move(handler); }

  // Shows the text for the current value after the label.
  auto set_status_text(std::string off_text, std::string on_text) -> void;
  auto clear_status_text() -> void;

protected:
  auto perform_activation() -> void override;
  [[nodiscard]] auto activates_on_enter() const -> bool override { return true; }
  [[nodiscard]] auto indicator_size(theme const &t) const -> basalt::size override;
  auto paint_indicator(render_context &context, rect box, theme const &t, control_state state) const
      -> void override;
  auto describe(accessibility::info &details) const -> void override;

private:
  auto refresh_status() -> void;

  bool on_ = false;
  bool has_status_ = false;
  std::string off_text_;
  std::string on_text_;
  std::function<void(bool)> on_change_;
};

} // namespace basalt
