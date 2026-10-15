#pragma once

#include "basalt/layout.h"
#include "basalt/widget.h"

#include <optional>

namespace basalt {

class split_view;

// The draggable divider of a `split_view` (see `split_view::set_resizable`).
// Its bounds are wider than the visible line so that it is easy to grab; the
// line itself is drawn in the middle.
//
// A primary press starts a drag that moves the divider with the pointer. The
// splitter is a Tab stop; Left and Right (or Up and Down for a stacked split)
// move the divider by a step, Home and End move it to either limit.
class splitter : public widget {
public:
  splitter(split_view &owner, axis direction);

  [[nodiscard]] auto accessibility_info() const -> accessibility::info override;
  [[nodiscard]] auto focus_ring() const -> std::optional<focus_ring_shape> override;

  // Layout pixels moved by one arrow key press.
  static constexpr length key_step = fixed(16);

protected:
  auto on_event(event_context &context, event const &e) -> bool override;
  auto on_paint(render_context &context) const -> void override;
  auto on_accessibility_action(accessibility::action_request const &request) -> bool override;

private:
  friend class split_view;

  auto set_direction(axis direction) -> void { direction_ = direction; }

  split_view *owner_;
  axis direction_;
  bool dragging_ = false;
  // Distance from the divider's leading edge to the pointer at press time.
  float grab_offset_ = 0;
};

} // namespace basalt
