#pragma once

#include "basalt/layout.h"
#include "basalt/widget.h"
#include "basalt/widgets/text_line.h"

#include <cstdint>
#include <functional>
#include <string>

namespace basalt {

enum class button_variant : std::uint8_t {
  // The main action of a view; filled with the accent color.
  primary,
  // An outlined button.
  secondary,
  // Text only, with a tinted background on hover.
  tertiary,
  // A destructive action; filled with the danger color.
  danger,
};

// A push button. It is a Tab stop and takes focus when clicked. A primary
// press that is released over the button, or Enter or Space while it has
// focus, calls the press handler. Disabled buttons stay focusable but do
// nothing.
class button : public widget {
public:
  explicit button(std::string label = {}, std::function<void()> on_press = {});

  [[nodiscard]] auto label() const -> std::string const & { return line_.text(); }
  auto set_label(std::string label) -> void;
  [[nodiscard]] auto variant() const -> button_variant { return variant_; }
  auto set_variant(button_variant variant) -> void;
  // Where the label sits in the button; centered by default.
  [[nodiscard]] auto label_alignment() const -> alignment { return alignment_; }
  auto set_label_alignment(alignment a) -> void;
  // Announced after the name, such as "Ctrl+S".
  [[nodiscard]] auto shortcut() const -> std::string const & { return shortcut_; }
  auto set_shortcut(std::string shortcut) -> void;
  auto set_on_press(std::function<void()> on_press) -> void { on_press_ = std::move(on_press); }

  // Calls the handler as a click would. False when the button is disabled.
  auto press() -> bool;

  [[nodiscard]] auto accessibility_info() const -> accessibility::info override;

protected:
  auto on_event(event_context &context, event const &e) -> bool override;
  auto on_paint(render_context &context) const -> void override;
  [[nodiscard]] auto on_measure(constraints const &limits) const -> basalt::size override;
  auto on_accessibility_action(accessibility::action_request const &request) -> bool override;

private:
  text_line line_;
  button_variant variant_ = button_variant::secondary;
  std::string shortcut_;
  alignment alignment_ = alignment::center;
  std::function<void()> on_press_;
};

} // namespace basalt
