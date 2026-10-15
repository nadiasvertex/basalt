#pragma once

#include "basalt/geometry.h"
#include "basalt/layout.h"
#include "basalt/units.h"
#include "basalt/widget.h"
#include "basalt/widgets/splitter.h"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

namespace basalt {

// What a `split_view` keeps constant when it is resized.
enum class split_anchor : std::uint8_t {
  // Each pane keeps its fraction of the space.
  proportional,
  // The first pane keeps its length; the second takes the rest.
  first,
  // The second pane keeps its length; the first takes the rest.
  second,
};

// Two panes separated by a divider. With `axis::horizontal` the panes sit
// side by side (a vertical divider); with `axis::vertical` they are stacked.
//
// The divider position follows `anchor()`. Anchored lengths are token
// lengths, so a fixed sidebar width follows zoom, and a relative one also
// follows the text scale. Pane minimums are enforced on every layout; when
// both cannot fit, the first pane's minimum wins. When only one pane is
// present and visible it fills the view and there is no divider.
//
// `set_resizable(true)` adds a `splitter` child over the divider for dragging
// and keyboard resizing; without it the divider is drawn but not interactive.
// The splitter is the last child, after the panes.
class split_view : public widget {
public:
  explicit split_view(axis direction = axis::horizontal) : direction_(direction) {}

  // Replace the pane, destroying the previous one.
  template <std::derived_from<widget> T> auto set_first(std::unique_ptr<T> pane) -> T & {
    T *const added = pane.get();
    replace_pane(first_id_, 0, std::unique_ptr<widget>(std::move(pane)));
    return *added;
  }
  template <std::derived_from<widget> T> auto set_second(std::unique_ptr<T> pane) -> T & {
    T *const added = pane.get();
    replace_pane(second_id_, child_count(), std::unique_ptr<widget>(std::move(pane)));
    return *added;
  }
  // Null when the pane is not set.
  [[nodiscard]] auto first() const -> widget *;
  [[nodiscard]] auto second() const -> widget *;

  [[nodiscard]] auto direction() const -> axis { return direction_; }
  auto set_direction(axis direction) -> void;

  [[nodiscard]] auto divider_thickness() const -> length { return divider_thickness_; }
  auto set_divider_thickness(length thickness) -> void;
  [[nodiscard]] auto first_minimum() const -> length { return first_minimum_; }
  [[nodiscard]] auto second_minimum() const -> length { return second_minimum_; }
  auto set_minimums(length first, length second) -> void;

  [[nodiscard]] auto anchor() const -> split_anchor { return anchor_; }
  // The first pane's share of the space; meaningful when proportional.
  [[nodiscard]] auto ratio() const -> float { return ratio_; }
  // The anchored pane's length; meaningful when not proportional.
  [[nodiscard]] auto anchored_length() const -> length { return anchored_; }
  // Makes the split proportional. `ratio` is clamped to [0, 1].
  auto set_ratio(float ratio) -> void;
  // Anchors the first or second pane at `pane_length`.
  auto set_first_length(length pane_length) -> void;
  auto set_second_length(length pane_length) -> void;

  // Moves the divider's leading edge to `position` (local layout pixels along
  // the split axis), keeping the current anchor kind and unit. Returns the
  // position after enforcing the minimums.
  auto move_divider_to(float position) -> float;
  auto move_divider_by(float delta) -> float;

  // As of the last layout; empty when there is no divider.
  [[nodiscard]] auto divider_rect() const -> rect { return divider_; }

  [[nodiscard]] auto resizable() const -> bool { return resizable_; }
  auto set_resizable(bool resizable) -> void;
  // The interactive divider; null unless resizable.
  [[nodiscard]] auto handle() const -> splitter *;

  static constexpr length default_divider_thickness = fixed(1);
  // The grab area of the splitter is at least this wide.
  static constexpr length minimum_grab_width = fixed(8);

protected:
  auto on_paint(render_context &context) const -> void override;
  auto on_layout() -> void override;
  [[nodiscard]] auto on_measure(constraints const &limits) const -> basalt::size override;
  auto on_child_removed(widget &child) -> void override;

private:
  auto replace_pane(widget_id &slot, std::size_t index, std::unique_ptr<widget> pane) -> void;
  [[nodiscard]] auto find_child(widget_id id) const -> widget *;
  // Space shared by the panes along the split axis.
  [[nodiscard]] auto available() const -> float;
  // Applies the minimums to a first-pane length.
  [[nodiscard]] auto limit_first(float first, float space) const -> float;
  [[nodiscard]] auto desired_first(float space) const -> float;

  axis direction_;
  widget_id first_id_;
  widget_id second_id_;
  widget_id handle_id_;
  bool resizable_ = false;
  length divider_thickness_ = default_divider_thickness;
  length first_minimum_;
  length second_minimum_;
  split_anchor anchor_ = split_anchor::proportional;
  float ratio_ = 0.5f;
  length anchored_;
  rect divider_;
};

} // namespace basalt
