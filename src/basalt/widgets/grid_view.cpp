#include "basalt/widgets/grid_view.h"

#include "basalt/render_context.h"
#include "basalt/widget_tree.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

namespace basalt {

namespace {

// `value` rounded down, as a count; negative values give 0.
auto floor_count(float value) -> std::size_t {
  return static_cast<std::size_t>(std::max(0.0F, std::floor(value)));
}

} // namespace

grid_view::grid_view(std::string name) : name_(std::move(name)) {}

auto grid_view::set_item_count(std::size_t count) -> void {
  if (count != item_count_) {
    item_count_ = count;
    invalidate_content();
  }
}

auto grid_view::set_cell_size(float size) -> void {
  size = std::max(0.0F, size);
  if (size != cell_size_) {
    cell_size_ = size;
    invalidate_content();
  }
}

auto grid_view::set_gap(float gap) -> void {
  gap = std::max(0.0F, gap);
  if (gap != gap_) {
    gap_ = gap;
    invalidate_content();
  }
}

auto grid_view::set_cell_painter(cell_painter painter) -> void {
  painter_ = std::move(painter);
  invalidate_paint();
}

auto grid_view::set_on_visible_range_changed(range_handler handler) -> void {
  on_visible_range_changed_ = std::move(handler);
}

auto grid_view::columns_for(float width) const -> std::size_t {
  if (pitch() <= 0) {
    return 1;
  }
  return std::max<std::size_t>(1, floor_count((width - gap_) / pitch()));
}

auto grid_view::columns() const -> std::size_t { return columns_for(bounds().extent.width); }

auto grid_view::rows() const -> std::size_t {
  auto const cols = columns();
  return (item_count_ + cols - 1) / cols;
}

auto grid_view::content_height(float width) const -> float {
  if (item_count_ == 0) {
    return 0;
  }
  auto const cols = columns_for(width);
  auto const row_count = (item_count_ + cols - 1) / cols;
  return gap_ + static_cast<float>(row_count) * pitch();
}

auto grid_view::cell_rect(std::size_t index) const -> rect {
  auto const cols = columns();
  auto const row = index / cols;
  auto const column = index % cols;
  return {{gap_ + static_cast<float>(column) * pitch(), gap_ + static_cast<float>(row) * pitch()},
          {cell_size_, cell_size_}};
}

auto grid_view::band(point local) const -> std::pair<long long, long long> {
  if (pitch() <= 0) {
    return {-1, -1};
  }
  return {static_cast<long long>(std::floor((local.x - gap_) / pitch())),
          static_cast<long long>(std::floor((local.y - gap_) / pitch()))};
}

auto grid_view::index_at(point local) const -> std::optional<std::size_t> {
  auto const [column, row] = band(local);
  auto const cols = static_cast<long long>(columns());
  if (column < 0 || row < 0 || column >= cols) {
    return std::nullopt;
  }
  auto const index = static_cast<std::size_t>(row * cols + column);
  if (index >= item_count_) {
    return std::nullopt;
  }
  auto const cell = cell_rect(index);
  if (local.x >= cell.right() || local.y >= cell.bottom()) {
    return std::nullopt;
  }
  return index;
}

auto grid_view::nearest_index(point local) const -> std::optional<std::size_t> {
  if (item_count_ == 0) {
    return std::nullopt;
  }
  auto const [column, row] = band(local);
  auto const cols = static_cast<long long>(columns());
  auto const last_row = static_cast<long long>(rows()) - 1;
  auto const index = static_cast<std::size_t>(std::clamp(row, 0LL, last_row) * cols +
                                              std::clamp(column, 0LL, cols - 1));
  return std::min(index, item_count_ - 1);
}

auto grid_view::indices_in(rect area) const -> std::vector<std::size_t> {
  std::vector<std::size_t> found;
  if (item_count_ == 0 || area.empty() || pitch() <= 0) {
    return found;
  }
  auto const cols = columns();
  auto const first_row = floor_count((area.top() - gap_) / pitch());
  auto const last_row = std::min(rows() - 1, floor_count((area.bottom() - gap_) / pitch()));
  auto const first_column = floor_count((area.left() - gap_) / pitch());
  auto const last_column = std::min(cols - 1, floor_count((area.right() - gap_) / pitch()));
  for (auto row = first_row; row <= last_row; ++row) {
    for (auto column = first_column; column <= last_column; ++column) {
      auto const index = row * cols + column;
      if (index < item_count_ && intersects(cell_rect(index), area)) {
        found.push_back(index);
      }
    }
  }
  return found;
}

auto grid_view::visible_area() const -> rect {
  auto area = rect{{}, bounds().extent};
  widget_tree const *const owner = tree();
  if (owner == nullptr) {
    return area;
  }
  // Everything below in root coordinates, moved so the grid's origin is (0, 0).
  auto const to_local = point{} - to_root({});
  area = intersection(area, translated(rect{{}, owner->viewport()}, to_local));
  for (widget const *ancestor = parent(); ancestor != nullptr; ancestor = ancestor->parent()) {
    area = intersection(area, translated(ancestor->bounds_in_root(), to_local));
  }
  return area;
}

auto grid_view::range_between(float top, float bottom) const -> index_range {
  if (item_count_ == 0 || bottom <= top || pitch() <= 0) {
    return {};
  }
  auto const cols = columns();
  auto const first_row = floor_count((top - gap_) / pitch());
  auto const end_row = std::min(
      rows(), static_cast<std::size_t>(std::max(0.0F, std::ceil((bottom - gap_) / pitch()))));
  auto const first = std::min(first_row * cols, item_count_);
  auto const last = std::min(end_row * cols, item_count_);
  return {first, std::max(first, last)};
}

auto grid_view::visible_range() const -> index_range {
  auto const area = visible_area();
  if (area.empty()) {
    return {};
  }
  return range_between(area.top(), area.bottom());
}

auto grid_view::invalidate_cell(std::size_t index) -> void {
  if (index >= item_count_) {
    return;
  }
  auto const cell = cell_rect(index);
  if (intersects(cell, visible_area())) {
    invalidate_paint(cell);
  }
}

auto grid_view::accessibility_info() const -> accessibility::info {
  accessibility::info details;
  details.kind = accessibility::role::list;
  details.name = name_;
  details.set_size = static_cast<std::uint32_t>(
      std::min<std::size_t>(item_count_, std::numeric_limits<std::uint32_t>::max()));
  return details;
}

auto grid_view::report_visible_range() -> void {
  auto const visible = visible_range();
  if (reported_ == visible) {
    return;
  }
  reported_ = visible;
  if (on_visible_range_changed_) {
    on_visible_range_changed_(visible);
  }
}

auto grid_view::on_measure(constraints const &limits) const -> basalt::size {
  auto const width = limits.bounded_width() ? limits.max.width : limits.min.width;
  return {width, content_height(width)};
}

auto grid_view::on_layout() -> void { report_visible_range(); }

auto grid_view::on_viewport_moved() -> void { report_visible_range(); }

auto grid_view::on_paint(render_context &context) const -> void {
  if (!painter_) {
    return;
  }
  auto const visible = visible_range();
  for (auto index = visible.first; index < visible.last; ++index) {
    painter_(context, index, cell_rect(index));
  }
}

} // namespace basalt
