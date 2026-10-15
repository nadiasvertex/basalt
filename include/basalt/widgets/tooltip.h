#pragma once

#include "basalt/widget.h"
#include "basalt/widget_id.h"
#include "basalt/widgets/hover_delay.h"
#include "basalt/widgets/text_line.h"

#include <chrono>
#include <string>

namespace basalt {

// The bubble of a tooltip: one line of text on a dark background. Show it
// with `tooltip_anchor`, or place it in an `overlay_host` directly.
class tooltip : public widget {
public:
  explicit tooltip(std::string text = {});

  [[nodiscard]] auto text() const -> std::string const & { return line_.text(); }
  auto set_text(std::string text) -> void;

  [[nodiscard]] auto accessibility_info() const -> accessibility::info override;

protected:
  auto on_paint(render_context &context) const -> void override;
  [[nodiscard]] auto on_measure(constraints const &limits) const -> basalt::size override;

private:
  text_line line_;
};

// Shows a tooltip for the widget that owns it after the pointer has rested on
// the widget or the widget has taken visible keyboard focus. The owner
// forwards two things:
//
//   - `on_interaction_changed` calls `update`, which returns true when the
//     owner should call `request_animation_frame`;
//   - `on_animation_frame` calls `advance` and returns its result.
//
// The tooltip hides when the pointer leaves, the focus ring goes away, the
// text becomes empty or the owner detaches (`hide`). Without an
// `overlay_host` above the owner nothing is shown.
class tooltip_anchor {
public:
  explicit tooltip_anchor(std::chrono::nanoseconds delay = hover_delay::default_delay)
      : delay_(delay) {}

  [[nodiscard]] auto text() const -> std::string const & { return text_; }
  // Ends a visible tooltip when the text becomes empty.
  auto set_text(widget &owner, std::string text) -> void;

  [[nodiscard]] auto visible() const -> bool { return shown_.valid(); }

  // True when the owner should request an animation frame.
  [[nodiscard]] auto update(widget &owner) -> bool;
  // True when the owner should request another animation frame.
  [[nodiscard]] auto advance(widget &owner, std::chrono::nanoseconds elapsed) -> bool;
  auto hide(widget &owner) -> void;

private:
  auto show(widget &owner) -> void;

  std::string text_;
  hover_delay delay_;
  widget_id shown_;
};

} // namespace basalt
