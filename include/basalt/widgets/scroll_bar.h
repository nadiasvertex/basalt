#pragma once

#include "basalt/layout.h"
#include "basalt/widget.h"

#include <functional>

namespace basalt {

// A scroll bar: a track with a draggable thumb. It does not scroll anything
// itself; it reports the offset the user asks for and its owner applies it
// (see `scroll_view::set_scrollbars`). Offsets, extents and the track are in
// layout pixels along the bar's axis.
//
// Pressing the thumb drags it; pressing the track beside the thumb pages
// toward the press. The bar is not a Tab stop: keyboard scrolling belongs to
// the scrolled content, and assistive technology uses the increment and
// decrement actions.
class scroll_bar : public widget {
public:
  explicit scroll_bar(axis direction) : direction_(direction) {}

  [[nodiscard]] auto direction() const -> axis { return direction_; }

  // `content` is the scrollable extent, `viewport` the visible part of it and
  // `offset` the start of the visible part.
  auto set_metrics(float content, float viewport, float offset) -> void;
  [[nodiscard]] auto content_extent() const -> float { return content_; }
  [[nodiscard]] auto viewport_extent() const -> float { return viewport_; }
  [[nodiscard]] auto offset() const -> float { return offset_; }
  [[nodiscard]] auto max_offset() const -> float;

  // The thumb's rectangle in this widget's coordinates; empty when there is
  // nothing to scroll.
  [[nodiscard]] auto thumb_rect() const -> rect;

  // Called with the requested offset, already clamped to [0, max_offset()].
  auto set_on_scroll(std::function<void(float)> handler) -> void {
    on_scroll_ = std::move(handler);
  }

  [[nodiscard]] auto accessibility_info() const -> accessibility::info override;

  static constexpr length default_thickness = fixed(12);
  static constexpr length minimum_thumb = fixed(24);

protected:
  auto on_event(event_context &context, event const &e) -> bool override;
  auto on_paint(render_context &context) const -> void override;
  auto on_accessibility_action(accessibility::action_request const &request) -> bool override;

private:
  [[nodiscard]] auto track_length() const -> float;
  [[nodiscard]] auto thumb_length() const -> float;
  [[nodiscard]] auto thumb_start() const -> float;
  auto request_offset(float offset) -> void;

  axis direction_;
  float content_ = 0;
  float viewport_ = 0;
  float offset_ = 0;
  bool dragging_ = false;
  // Pointer position along the axis, and the offset, when the drag began.
  float drag_origin_ = 0;
  float drag_offset_ = 0;
  std::function<void(float)> on_scroll_;
};

} // namespace basalt
