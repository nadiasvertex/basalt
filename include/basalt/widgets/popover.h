#pragma once

#include "basalt/widget.h"
#include "basalt/widgets/hover_delay.h"

#include <chrono>
#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace basalt {

// How a popover is opened.
enum class popover_trigger : std::uint8_t {
  // Hovering or focusing the trigger for a moment shows short, non-interactive
  // text. The pointer passes through to the content beneath.
  hover,
  // A click, Enter or Space on the trigger shows content that can be operated.
  // It takes focus and closes on an outside press or Escape.
  click,
};

// A trigger widget and a panel of content shown beside it in an
// `overlay_host`. The panel opens below the trigger, or above it when it does
// not fit, and shows an optional tip that points at the trigger. Its width
// follows the content between the popover width tokens. Only one popover is
// open in a host at a time: opening one closes another.
//
// The popover reacts to what reaches it from the trigger: hover and focus for
// the hover trigger, and presses, Enter and Space that the trigger leaves
// unhandled for the click trigger. A trigger that handles clicks itself, such
// as a button, calls `toggle` from its own handler.
class popover : public widget {
public:
  explicit popover(popover_trigger trigger = popover_trigger::click);
  ~popover() override;

  template <std::derived_from<widget> T> auto set_trigger(std::unique_ptr<T> trigger) -> T & {
    T &added = *trigger;
    replace_trigger(std::unique_ptr<widget>(std::move(trigger)));
    return added;
  }
  template <std::derived_from<widget> T, class... Args>
  auto emplace_trigger(Args &&...args) -> T & {
    return set_trigger(std::make_unique<T>(std::forward<Args>(args)...));
  }

  [[nodiscard]] auto trigger() const -> popover_trigger { return trigger_; }
  auto set_trigger_kind(popover_trigger trigger) -> void;

  // Content is built each time the popover opens.
  auto set_content(std::function<std::unique_ptr<widget>()> factory) -> void;
  // Shows plain text.
  auto set_text(std::string text) -> void;

  [[nodiscard]] auto tip() const -> bool { return tip_; }
  auto set_tip(bool tip) -> void { tip_ = tip; }
  // How long the pointer rests before a hover popover opens.
  auto set_hover_delay(std::chrono::nanoseconds delay) -> void { delay_ = hover_delay{delay}; }
  // Called with true when the panel opens and false when it closes.
  auto set_on_visibility_changed(std::function<void(bool)> handler) -> void {
    on_visibility_ = std::move(handler);
  }

  [[nodiscard]] auto is_open() const -> bool;
  // False when there is no content, no host, or the popover is disabled.
  auto open() -> bool;
  auto close() -> void;
  auto toggle() -> bool;

protected:
  auto on_event(event_context &context, event const &e) -> bool override;
  auto on_layout() -> void override;
  [[nodiscard]] auto on_measure(constraints const &limits) const -> basalt::size override;
  auto on_interaction_changed(interaction_state const &previous) -> void override;
  auto on_animation_frame(std::chrono::nanoseconds elapsed) -> bool override;
  auto on_detached() -> void override;

private:
  auto replace_trigger(std::unique_ptr<widget> trigger) -> void;
  auto panel_closed() -> void;

  popover_trigger trigger_;
  std::function<std::unique_ptr<widget>()> content_;
  bool tip_ = true;
  hover_delay delay_;
  widget_id panel_;
  std::function<void(bool)> on_visibility_;
};

} // namespace basalt
