#pragma once

#include "basalt/layout.h"
#include "basalt/theme.h"
#include "basalt/units.h"
#include "basalt/widget.h"

#include <concepts>
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>

namespace basalt {

// How a `box` sizes and places one child.
struct box_item {
  // Share of spare main-axis space.
  float grow = 0;
  // Share of a main-axis shortfall, weighted by the child's basis.
  float shrink = 1;
  // Main-axis size before growing or shrinking; the measured size when unset.
  std::optional<length> basis;
  length min;
  // Unbounded when unset.
  std::optional<length> max;
  // Overrides the box's `cross_alignment` for this child.
  std::optional<alignment> align;

  [[nodiscard]] friend auto operator==(box_item const &, box_item const &) -> bool = default;
};

// A background and border painted behind a box's children. The colors are theme tokens, so
// the surface follows the color scheme.
struct box_surface {
  rgba tokens::color_set::*fill = &tokens::color_set::background_default;
  rgba tokens::color_set::*border = &tokens::color_set::border_default;
  length radius = fixed(4);

  [[nodiscard]] friend auto operator==(box_surface const &, box_surface const &) -> bool = default;
};

// Arranges its visible children in a row (`axis::horizontal`) or a column
// (`axis::vertical`), separated by `gap` and inset by `padding`, in child
// order. Main-axis sizes come from `distribute`; spare space not taken by
// growing children is placed according to `main_alignment`. Child edges are
// snapped to device pixels, so neighbours still abut. A box paints nothing
// unless it has a surface.
class box : public widget {
public:
  explicit box(axis direction = axis::vertical) : direction_(direction) {}

  // Adds `child` laid out according to `item`.
  template <std::derived_from<widget> T> auto add(std::unique_ptr<T> child, box_item item) -> T & {
    T &added = add_child(std::move(child));
    set_item(added, std::move(item));
    return added;
  }

  [[nodiscard]] auto direction() const -> axis { return direction_; }
  auto set_direction(axis direction) -> void;
  [[nodiscard]] auto gap() const -> length { return gap_; }
  auto set_gap(length gap) -> void;
  [[nodiscard]] auto padding() const -> length_insets const & { return padding_; }
  auto set_padding(length_insets padding) -> void;
  [[nodiscard]] auto main_alignment() const -> alignment { return main_alignment_; }
  // `stretch` behaves like `start`; use `box_item::grow` to fill space.
  auto set_main_alignment(alignment a) -> void;
  [[nodiscard]] auto cross_alignment() const -> alignment { return cross_alignment_; }
  auto set_cross_alignment(alignment a) -> void;

  [[nodiscard]] auto surface() const -> std::optional<box_surface> const & { return surface_; }
  auto set_surface(std::optional<box_surface> surface) -> void;

  // The default item for children without one.
  [[nodiscard]] auto item(widget const &child) const -> box_item;
  // False when `child` is not a child of this box.
  auto set_item(widget const &child, box_item item) -> bool;

protected:
  auto on_layout() -> void override;
  [[nodiscard]] auto on_measure(constraints const &limits) const -> basalt::size override;
  auto on_child_removed(widget &child) -> void override;
  auto on_paint(render_context &context) const -> void override;

private:
  [[nodiscard]] auto child_limits(float cross_max) const -> constraints;
  [[nodiscard]] auto preferred_main(box_item const &item, basalt::size measured) const -> float;

  axis direction_;
  length gap_;
  length_insets padding_;
  alignment main_alignment_ = alignment::start;
  alignment cross_alignment_ = alignment::stretch;
  std::optional<box_surface> surface_;
  std::unordered_map<widget_id, box_item> items_;
};

} // namespace basalt
