#include "basalt/widgets/grid_layout.h"

#include <algorithm>
#include <cmath>
#include <ranges>

namespace basalt {

grid_layout::grid_layout(float width, basalt::size cell, float gap, float header_height,
                         std::span<grid_section const> sections)
    : cell_(cell), gap_(std::max(0.0f, gap)), header_height_(std::max(0.0f, header_height)) {
  float const pitch = cell_.width + gap_;
  columns_ =
      pitch > 0
          ? std::max<std::size_t>(1, static_cast<std::size_t>(std::floor((width + gap_) / pitch)))
          : 1;
  sections_.reserve(sections.size());
  float y = 0;
  for (grid_section const &s : sections) {
    section_info info;
    info.first = item_count_;
    info.count = s.count;
    info.rows = (s.count + columns_ - 1) / columns_;
    info.header = s.header;
    info.top = y;
    y += s.header ? header_height_ + (s.count > 0 ? gap_ : 0) : 0;
    info.items_top = y;
    y += info.rows > 0 ? static_cast<float>(info.rows) * row_pitch() - gap_ : 0;
    info.bottom = y;
    if (&s != &sections.back() && (info.rows > 0 || s.header)) {
      y += gap_;
    }
    item_count_ += s.count;
    sections_.push_back(info);
  }
  total_height_ = y;
}

auto grid_layout::section_of(std::size_t index) const -> std::size_t {
  // Empty sections share their first index with the section after them, and
  // `upper_bound` lands after all of them, so the section before the result is
  // the one that holds `index`.
  auto const after = std::ranges::upper_bound(sections_, index, {}, &section_info::first);
  return static_cast<std::size_t>(after - sections_.begin()) - 1;
}

auto grid_layout::item_rect(std::size_t index) const -> rect {
  section_info const &s = sections_[section_of(index)];
  std::size_t const local = index - s.first;
  float const x = static_cast<float>(local % columns_) * (cell_.width + gap_);
  float const y = s.items_top + static_cast<float>(local / columns_) * row_pitch();
  return {{x, y}, cell_};
}

auto grid_layout::header_rect(std::size_t section) const -> rect {
  section_info const &s = sections_[section];
  if (!s.header) {
    return {};
  }
  float const width = static_cast<float>(columns_) * (cell_.width + gap_) - gap_;
  return {{0, s.top}, {width, header_height_}};
}

auto grid_layout::section_items(std::size_t section) const -> index_range {
  section_info const &s = sections_[section];
  return {s.first, s.first + s.count};
}

auto grid_layout::section_at(float y) const -> std::size_t {
  auto const after = std::ranges::upper_bound(sections_, y, {}, &section_info::bottom);
  std::size_t const section = static_cast<std::size_t>(after - sections_.begin());
  return std::min(section, sections_.size() - 1);
}

auto grid_layout::row_start(std::size_t index) const -> std::size_t {
  section_info const &s = sections_[section_of(index)];
  std::size_t const local = index - s.first;
  return s.first + local - local % columns_;
}

auto grid_layout::row_end(std::size_t index) const -> std::size_t {
  section_info const &s = sections_[section_of(index)];
  std::size_t const row = (index - s.first) / columns_;
  return s.first + std::min(s.count, (row + 1) * columns_);
}

auto grid_layout::visible_items(float top, float bottom, std::size_t margin_rows) const
    -> index_range {
  if (item_count_ == 0 || bottom <= top || top >= total_height_) {
    return {};
  }
  // The first row that ends below `top`. A `top` above the items of a section
  // starts at its first row, and the search moves on past sections without items.
  std::optional<std::size_t> first;
  for (std::size_t i = section_at(top); i < sections_.size(); ++i) {
    section_info const &s = sections_[i];
    if (s.count == 0) {
      continue;
    }
    float const local = std::max(0.0f, top - s.items_top);
    auto row = static_cast<std::size_t>(local / row_pitch());
    // A `top` in the gap under a row does not reach that row.
    if (local - static_cast<float>(row) * row_pitch() >= cell_.height) {
      ++row;
    }
    if (row >= s.rows) {
      continue;
    }
    first = s.first + row * columns_;
    break;
  }
  if (!first) {
    return {};
  }
  // The last row that starts above `bottom`. `bottom` may lie in a header or a
  // gap, so the search runs backward to the nearest section with rows above it.
  std::optional<std::size_t> last;
  for (std::size_t i = section_at(bottom) + 1; i-- > 0;) {
    section_info const &s = sections_[i];
    if (s.count > 0 && s.items_top < bottom) {
      auto const started =
          static_cast<std::size_t>(std::ceil((bottom - s.items_top) / row_pitch()));
      last = s.first + std::min(s.count, std::min(s.rows, started) * columns_);
      break;
    }
  }
  if (!last || *last <= *first) {
    return {};
  }
  std::size_t begin = *first;
  std::size_t end = *last;
  for (std::size_t n = 0; n < margin_rows; ++n) {
    if (auto const above = item_above(begin)) {
      begin = row_start(*above);
    }
    if (auto const below = item_below(end - 1)) {
      end = row_end(*below);
    }
  }
  return {begin, end};
}

auto grid_layout::visible_headers(float top, float bottom) const -> index_range {
  if (sections_.empty() || bottom <= top) {
    return {};
  }
  std::size_t first = sections_.size();
  std::size_t last = 0;
  for (std::size_t i = section_at(top); i < sections_.size(); ++i) {
    section_info const &s = sections_[i];
    if (s.top >= bottom) {
      break;
    }
    if (s.header && s.top + header_height_ > top) {
      first = std::min(first, i);
      last = i + 1;
    }
  }
  return first < last ? index_range{first, last} : index_range{};
}

auto grid_layout::item_above(std::size_t index) const -> std::optional<std::size_t> {
  std::size_t const si = section_of(index);
  section_info const &s = sections_[si];
  std::size_t const local = index - s.first;
  if (local >= columns_) {
    return index - columns_;
  }
  for (std::size_t i = si; i-- > 0;) {
    section_info const &p = sections_[i];
    if (p.count > 0) {
      std::size_t const column = local % columns_;
      std::size_t const last_row_start = (p.rows - 1) * columns_;
      return p.first + std::min(p.count - 1, last_row_start + column);
    }
  }
  return std::nullopt;
}

auto grid_layout::item_below(std::size_t index) const -> std::optional<std::size_t> {
  std::size_t const si = section_of(index);
  section_info const &s = sections_[si];
  std::size_t const local = index - s.first;
  if (local + columns_ < s.count) {
    return index + columns_;
  }
  if (local / columns_ + 1 < s.rows) {
    // The next row is shorter than the column; land on its last item.
    return s.first + s.count - 1;
  }
  for (std::size_t i = si + 1; i < sections_.size(); ++i) {
    section_info const &n = sections_[i];
    if (n.count > 0) {
      return n.first + std::min(n.count - 1, local % columns_);
    }
  }
  return std::nullopt;
}

} // namespace basalt
