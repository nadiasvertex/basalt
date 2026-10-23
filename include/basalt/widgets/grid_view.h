#pragma once

#include "basalt/geometry.h"
#include "basalt/index_range.h"
#include "basalt/layout.h"
#include "basalt/widget.h"

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace basalt {

class render_context;

// A uniform grid of square cells for a number of items, such as a wall of thumbnails.
//
// Cells go left to right and then top to bottom. `gap` separates the cells and also surrounds
// them, so the first cell starts at (gap, gap). The grid fits as many columns into its width as
// it can, and at least one. Its height is the height of its rows, so put it in a vertical
// `scroll_view` when the rows do not fit.
//
// The grid has no child widgets. It paints each cell through the cell painter and paints only
// the cells in the part of the grid that its ancestors show, so a grid of millions of items
// costs only the cells on the screen. As the content of a `scroll_view`, it also reports a
// changed visible range while the view scrolls, which lets the application load the items it
// is about to show.
//
// Sizes are in layout pixels. Item data, selection and input stay with the application:
// `index_at`, `nearest_index` and `indices_in` turn pointer positions into items.
//
// For assistive technology the grid is one list with `name` and the number of items. The items
// are painted, not widgets, so they are not in the accessibility tree.
class grid_view : public widget {
public:
  // Paints item `index` into `cell`, in the grid's coordinates. Clipped to the grid.
  using cell_painter = std::function<void(render_context &context, std::size_t index, rect cell)>;
  using range_handler = std::function<void(index_range visible)>;

  explicit grid_view(std::string name = "Grid");

  [[nodiscard]] auto item_count() const -> std::size_t { return item_count_; }
  auto set_item_count(std::size_t count) -> void;

  // The side of a cell. Negative values become 0.
  [[nodiscard]] auto cell_size() const -> float { return cell_size_; }
  auto set_cell_size(float size) -> void;

  // The space between and around the cells. Negative values become 0.
  [[nodiscard]] auto gap() const -> float { return gap_; }
  auto set_gap(float gap) -> void;

  auto set_cell_painter(cell_painter painter) -> void;
  // Called after a scroll or a layout that changed `visible_range()`.
  auto set_on_visible_range_changed(range_handler handler) -> void;

  // The distance from one cell to the next: the cell size plus the gap.
  [[nodiscard]] auto pitch() const -> float { return cell_size_ + gap_; }
  // The number of columns that fit in `width`, at least 1.
  [[nodiscard]] auto columns_for(float width) const -> std::size_t;
  // For the current width.
  [[nodiscard]] auto columns() const -> std::size_t;
  [[nodiscard]] auto rows() const -> std::size_t;
  // The height of all the rows at `width`, with the outer gaps; 0 when there are no items.
  [[nodiscard]] auto content_height(float width) const -> float;

  // The cell of item `index`, in the grid's coordinates.
  [[nodiscard]] auto cell_rect(std::size_t index) const -> rect;
  // The item whose cell (not a gap) contains `local`, if any.
  [[nodiscard]] auto index_at(point local) const -> std::optional<std::size_t>;
  // The item whose cell and the gaps after it contain `local`, clamped into the grid. Nothing
  // when there are no items. Useful as the anchor of a zoom, where the pointer can be over a gap
  // or past the last item.
  [[nodiscard]] auto nearest_index(point local) const -> std::optional<std::size_t>;
  // Every item whose cell intersects `area`, in index order, for a selection rectangle.
  [[nodiscard]] auto indices_in(rect area) const -> std::vector<std::size_t>;

  // The part of the grid that its ancestors and the window show, in the grid's coordinates.
  [[nodiscard]] auto visible_area() const -> rect;
  // The items whose rows touch `visible_area()`.
  [[nodiscard]] auto visible_range() const -> index_range;

  // Repaints the cell of item `index` when it is visible, for example when its data arrives.
  auto invalidate_cell(std::size_t index) -> void;

  [[nodiscard]] auto accessibility_info() const -> accessibility::info override;

protected:
  auto on_paint(render_context &context) const -> void override;
  [[nodiscard]] auto on_measure(constraints const &limits) const -> basalt::size override;
  auto on_layout() -> void override;
  auto on_viewport_moved() -> void override;

private:
  // The pitch bands (column, row) that contain `local`; may be negative or past the end.
  [[nodiscard]] auto band(point local) const -> std::pair<long long, long long>;
  // The items whose rows touch the rows from `top` to `bottom`.
  [[nodiscard]] auto range_between(float top, float bottom) const -> index_range;
  auto report_visible_range() -> void;

  std::string name_;
  std::size_t item_count_ = 0;
  float cell_size_ = 128;
  float gap_ = 8;
  cell_painter painter_;
  range_handler on_visible_range_changed_;
  std::optional<index_range> reported_;
};

} // namespace basalt
