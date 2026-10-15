#pragma once

#include "basalt/geometry.h"
#include "basalt/layout.h"
#include "basalt/widget.h"
#include "basalt/widgets/scroll_bar.h"

#include <chrono>
#include <concepts>
#include <cstdint>
#include <memory>
#include <utility>

namespace basalt {

enum class scroll_axes : std::uint8_t { vertical, horizontal, both };

// Shows a window onto one content widget that may be larger than the view.
//
// Along each scrolling axis the content is measured without a limit and is at
// least as large as the view; along a non-scrolling axis it matches the view.
// The content is positioned at `-scroll_offset()`, and is clipped to the view
// like every child.
//
// Offsets are in layout pixels, snapped to device pixels, and kept within
// [0, max_scroll_offset()]. While this view awaits layout, requested offsets
// are kept as given and clamped once the content size is known, so callers
// may scroll to content they have just added.
//
// Input and scrollbars are opt-in layers on top of this. The wheel always
// scrolls, and it glides: each wheel event moves a destination, and the offset
// eases toward it over the following frames, so the content starts moving at
// once and slows to rest. The keyboard, the scroll bars and the programmatic
// calls above move at once. `set_scrollbars(true)` adds a `scroll_bar` along each scrolling
// axis, drawn over the content's edge and shown only while the content
// overflows. A scroll view that has keyboard focus (give it a focus policy)
// scrolls with the arrow, Page Up, Page Down, Home and End keys.
class scroll_view : public widget {
public:
  explicit scroll_view(scroll_axes axes = scroll_axes::vertical) : axes_(axes) {}

  // Replaces the content, destroying the previous one.
  template <std::derived_from<widget> T> auto set_content(std::unique_ptr<T> content) -> T & {
    T *const added = content.get();
    replace_content(std::unique_ptr<widget>(std::move(content)));
    return *added;
  }
  template <std::derived_from<widget> T, class... Args>
  auto emplace_content(Args &&...args) -> T & {
    return set_content(std::make_unique<T>(std::forward<Args>(args)...));
  }
  // Null when there is no content.
  [[nodiscard]] auto content() const -> widget *;

  [[nodiscard]] auto axes() const -> scroll_axes { return axes_; }
  auto set_axes(scroll_axes axes) -> void;
  [[nodiscard]] auto scrolls(axis a) const -> bool;

  // As of the last layout.
  [[nodiscard]] auto content_size() const -> basalt::size { return content_size_; }
  [[nodiscard]] auto viewport_size() const -> basalt::size { return bounds().extent; }
  [[nodiscard]] auto max_scroll_offset() const -> point;
  [[nodiscard]] auto scroll_offset() const -> point { return offset_; }
  // The part of the content currently shown, in content coordinates.
  [[nodiscard]] auto visible_content_rect() const -> rect { return {offset_, viewport_size()}; }

  // Returns true when the offset changed. Moves at once and stops any glide
  // that is under way.
  auto set_scroll_offset(point offset) -> bool;
  auto scroll_by(point delta) -> bool;
  // Scrolls as little as possible to show `area` (in content coordinates);
  // when it is larger than the view, its start edges are shown.
  auto scroll_into_view(rect area) -> bool;

  // Adds or removes the scroll bars. They are the last children, so they sit
  // in front of the content.
  [[nodiscard]] auto scrollbars() const -> bool { return scrollbars_; }
  auto set_scrollbars(bool enabled) -> void;
  // The bar along `a`; null unless scroll bars are enabled. A bar is hidden
  // while its axis does not scroll or the content fits.
  [[nodiscard]] auto scrollbar(axis a) const -> scroll_bar *;

protected:
  auto on_event(event_context &context, event const &e) -> bool override;
  auto on_layout() -> void override;
  [[nodiscard]] auto on_measure(constraints const &limits) const -> basalt::size override;
  auto on_child_removed(widget &child) -> void override;
  auto on_animation_frame(std::chrono::nanoseconds elapsed) -> bool override;

private:
  auto replace_content(std::unique_ptr<widget> content) -> void;
  [[nodiscard]] auto content_limits(basalt::size view) const -> constraints;
  [[nodiscard]] auto clamped(point offset) const -> point;
  // Moves the offset without ending a glide.
  auto apply_offset(point offset) -> bool;
  // Moves the glide's destination by `delta`; false when it cannot move.
  auto glide_by(point delta) -> bool;
  auto place_content() -> void;
  auto update_bars() -> void;
  [[nodiscard]] auto find_child(widget_id id) const -> widget *;

  scroll_axes axes_;
  widget_id content_id_;
  widget_id vertical_bar_id_;
  widget_id horizontal_bar_id_;
  bool scrollbars_ = false;
  basalt::size content_size_;
  point offset_;
  // The offset a glide is easing toward; meaningful only while `gliding_`.
  point glide_target_;
  bool gliding_ = false;
};

} // namespace basalt
