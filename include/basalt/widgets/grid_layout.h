#pragma once

#include "basalt/geometry.h"
#include "basalt/index_range.h"

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace basalt {

// One run of items in a grid. A section with a header gets a full-width row
// above its items.
struct grid_section {
  std::size_t count = 0;
  bool header = false;
};

// Where the items and headers of a grid go. It is a value computed from a
// width, a cell size and the sections, and it does not touch widgets, so it
// answers questions about a collection of any size without creating a widget.
//
// Items are numbered across all sections, in order. Each section starts on its
// own row. Cells have a fixed size and sit at the left of the row, separated by
// `gap` in both directions. The number of columns is the most that fit in the
// width, and at least one. Sizes are in layout pixels.
class grid_layout {
public:
  grid_layout() = default;
  grid_layout(float width, basalt::size cell, float gap, float header_height,
              std::span<grid_section const> sections);

  [[nodiscard]] auto columns() const -> std::size_t { return columns_; }
  [[nodiscard]] auto item_count() const -> std::size_t { return item_count_; }
  [[nodiscard]] auto section_count() const -> std::size_t { return sections_.size(); }
  [[nodiscard]] auto total_height() const -> float { return total_height_; }

  // Precondition: `index < item_count()`.
  [[nodiscard]] auto item_rect(std::size_t index) const -> rect;
  // The section that holds `index`. Precondition: `index < item_count()`.
  [[nodiscard]] auto section_of(std::size_t index) const -> std::size_t;
  // Empty when the section has no header. Precondition: `section < section_count()`.
  [[nodiscard]] auto header_rect(std::size_t section) const -> rect;
  // The first item of `section`, and the item after its last.
  [[nodiscard]] auto section_items(std::size_t section) const -> index_range;

  // The items in the rows that overlap the vertical span [top, bottom), widened
  // by `margin_rows` rows on each side. The range covers whole rows. A margin
  // that reaches past a section's last row continues into the next section.
  [[nodiscard]] auto visible_items(float top, float bottom, std::size_t margin_rows = 0) const
      -> index_range;
  // The sections whose headers overlap [top, bottom).
  [[nodiscard]] auto visible_headers(float top, float bottom) const -> index_range;

  // Keyboard movement. The result is empty at the edge of the grid. Moving
  // between rows keeps the column when the target row has one, and otherwise
  // lands on the last item of that row.
  [[nodiscard]] auto item_above(std::size_t index) const -> std::optional<std::size_t>;
  [[nodiscard]] auto item_below(std::size_t index) const -> std::optional<std::size_t>;

private:
  struct section_info {
    std::size_t first = 0;
    std::size_t count = 0;
    std::size_t rows = 0;
    bool header = false;
    // Where the section starts, including its header, and where its first row
    // of items starts.
    float top = 0;
    float items_top = 0;
    float bottom = 0;
  };

  [[nodiscard]] auto row_pitch() const -> float { return cell_.height + gap_; }
  // The section whose vertical extent holds `y`, clamped to the last section.
  [[nodiscard]] auto section_at(float y) const -> std::size_t;
  // The first item of the row that holds `index`, and the item after its last.
  [[nodiscard]] auto row_start(std::size_t index) const -> std::size_t;
  [[nodiscard]] auto row_end(std::size_t index) const -> std::size_t;

  basalt::size cell_;
  float gap_ = 0;
  float header_height_ = 0;
  std::size_t columns_ = 1;
  std::size_t item_count_ = 0;
  float total_height_ = 0;
  std::vector<section_info> sections_;
};

} // namespace basalt
