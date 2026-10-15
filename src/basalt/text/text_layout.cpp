#include "basalt/text/text_layout.h"

#include <algorithm>
#include <cmath>

namespace basalt::text {
namespace {

// Rough width of an average character, as a fraction of the font size, for
// estimating the height of blocks that have not been shaped yet.
constexpr auto estimated_em_per_character = 0.55F;
constexpr auto estimated_line_height_em = 1.3F;
// How far a selected block break extends, as a fraction of the font size.
constexpr auto break_marker_em = 0.25F;

} // namespace

text_layout::text_layout(text_shaper const &shaper, std::string text, text_layout_options options)
    : shaper_(&shaper), text_(std::move(text)), options_(std::move(options)) {
  blocks_ = split_blocks(0, text_);
}

auto text_layout::split_blocks(std::size_t base, std::string_view text) const
    -> std::vector<block> {
  auto result = std::vector<block>{};
  auto begin = std::size_t{0};
  for (;;) {
    auto const newline = text.find('\n', begin);
    auto end = newline == std::string_view::npos ? text.size() : newline;
    auto const next = end;
    if (newline != std::string_view::npos && end > begin && text[end - 1] == '\r') {
      --end;
    }
    result.push_back({base + begin, base + end, 0, false, 0, nullptr});
    if (newline == std::string_view::npos) {
      break;
    }
    begin = next + 1;
  }
  return result;
}

auto text_layout::clear_measurements() -> void {
  for (auto &item : blocks_) {
    item.measured = false;
    item.height = 0;
    item.laid_out.reset();
  }
  cached_.clear();
  tops_valid_ = 0;
}

auto text_layout::reset_measurements() -> void {
  clear_measurements();
  kept_.clear();
}

auto text_layout::keep_current_width() -> void {
  auto kept = kept_width{width_, {}, std::move(cached_)};
  kept.blocks.reserve(blocks_.size());
  for (auto &item : blocks_) {
    kept.blocks.push_back({item.height, item.measured, item.last_used, std::move(item.laid_out)});
  }
  cached_.clear();
  kept_.push_back(std::move(kept));
  if (kept_.size() > max_kept_widths) {
    kept_.erase(kept_.begin());
  }
}

auto text_layout::restore_width(kept_width &kept) -> void {
  for (auto i = std::size_t{0}; i < blocks_.size(); ++i) {
    auto &item = blocks_[i];
    auto &saved = kept.blocks[i];
    item.height = saved.height;
    item.measured = saved.measured;
    item.last_used = saved.last_used;
    item.laid_out = std::move(saved.laid_out);
  }
  cached_ = std::move(kept.cached);
  tops_valid_ = 0;
}

auto text_layout::set_width(float width) -> void {
  if (width == width_) {
    return;
  }
  // Nothing measured means nothing to keep.
  if (std::ranges::any_of(blocks_, &block::measured)) {
    keep_current_width();
  }
  width_ = width;
  auto const found = std::ranges::find(kept_, width, &kept_width::width);
  if (found == kept_.end()) {
    clear_measurements();
    return;
  }
  auto kept = std::move(*found);
  kept_.erase(found);
  restore_width(kept);
}

auto text_layout::set_options(text_layout_options options) -> void {
  options_ = std::move(options);
  reset_measurements();
}

auto text_layout::invalidate() -> void { reset_measurements(); }

auto text_layout::invalidate_block(std::size_t block) -> void {
  if (block >= blocks_.size()) {
    return;
  }
  // The measured height stays until the block is laid out again, which also repositions the
  // blocks below it.
  blocks_[block].laid_out.reset();
  std::erase(cached_, block);
  kept_.clear();
}

auto text_layout::options_for(std::size_t index) const -> paragraph_options {
  auto const &item = blocks_[index];
  auto result = options_.block_options
                    ? options_.block_options(
                          index, std::string_view{text_}.substr(item.begin, item.end - item.begin))
                    : options_.paragraph;
  result.max_width = width_;
  return result;
}

auto text_layout::estimate_height(std::size_t index) const -> float {
  auto const &style = options_.paragraph.shape.style;
  auto const line =
      style.line_height > 0 ? style.line_height : style.font_size * estimated_line_height_em;
  auto lines = 1.0F;
  if (options_.paragraph.wrap && std::isfinite(width_) && width_ > 0) {
    auto const length = static_cast<float>(blocks_[index].end - blocks_[index].begin);
    lines =
        std::max(1.0F, std::ceil(length * style.font_size * estimated_em_per_character / width_));
  }
  return lines * line + footer_of(index);
}

auto text_layout::footer_of(std::size_t index) const -> float {
  return options_.block_footer ? std::max(0.0F, options_.block_footer(index)) : 0.0F;
}

auto text_layout::ensure_tops(std::size_t upto) -> void {
  upto = std::min(upto, blocks_.size() - 1);
  if (tops_.size() < blocks_.size()) {
    tops_.resize(blocks_.size());
  }
  for (; tops_valid_ <= upto; ++tops_valid_) {
    auto const i = tops_valid_;
    if (!blocks_[i].measured) {
      blocks_[i].height = estimate_height(i);
    }
    tops_[i] = i == 0 ? 0.0F : tops_[i - 1] + blocks_[i - 1].height + options_.block_spacing;
  }
}

auto text_layout::measure(std::size_t index) -> std::shared_ptr<paragraph const> {
  auto &item = blocks_[index];
  item.last_used = ++clock_;
  if (item.laid_out) {
    return item.laid_out;
  }
  auto const view = std::string_view{text_}.substr(item.begin, item.end - item.begin);
  auto result = std::make_shared<paragraph const>(*shaper_, view, options_for(index));
  auto const height = result->extent().height + footer_of(index);
  if (!item.measured || item.height != height) {
    item.height = height;
    item.measured = true;
    // Blocks below move; this block's own top does not.
    tops_valid_ = std::min(tops_valid_, index + 1);
  }
  item.laid_out = result;
  cached_.push_back(index);
  if (cached_.size() > std::max<std::size_t>(2, options_.max_cached_paragraphs)) {
    evict();
  }
  return result;
}

auto text_layout::evict() -> void {
  std::ranges::sort(cached_, {}, [&](std::size_t i) { return blocks_[i].last_used; });
  auto const keep = std::max<std::size_t>(1, options_.max_cached_paragraphs / 2);
  auto const drop = cached_.size() - std::min(keep, cached_.size());
  for (auto i = std::size_t{0}; i < drop; ++i) {
    blocks_[cached_[i]].laid_out.reset();
  }
  cached_.erase(cached_.begin(), cached_.begin() + static_cast<std::ptrdiff_t>(drop));
}

auto text_layout::total_height() -> float {
  ensure_tops(blocks_.size() - 1);
  return tops_.back() + blocks_.back().height;
}

auto text_layout::block_begin(std::size_t block) const -> std::size_t {
  return blocks_[block].begin;
}

auto text_layout::block_end(std::size_t block) const -> std::size_t { return blocks_[block].end; }

auto text_layout::block_top(std::size_t block) -> float {
  ensure_tops(block);
  return tops_[block];
}

auto text_layout::block_height(std::size_t block) -> float {
  ensure_tops(block);
  return blocks_[block].height;
}

auto text_layout::is_measured(std::size_t block) const -> bool { return blocks_[block].measured; }

auto text_layout::block_at_offset(std::size_t offset) const -> std::size_t {
  auto const after = std::ranges::upper_bound(blocks_, offset, {}, &block::begin);
  auto const index = static_cast<std::size_t>(after - blocks_.begin());
  return index == 0 ? 0 : index - 1;
}

auto text_layout::block_at_y(float y) -> std::size_t {
  ensure_tops(blocks_.size() - 1);
  auto const after = std::upper_bound(
      tops_.begin(), tops_.begin() + static_cast<std::ptrdiff_t>(blocks_.size()), y);
  auto const index = static_cast<std::size_t>(after - tops_.begin());
  return index == 0 ? 0 : index - 1;
}

auto text_layout::layout_range(float top, float bottom) -> block_range {
  for (;;) {
    auto const first = block_at_y(top);
    auto last = block_at_y(std::max(top, bottom));
    if (last > first && tops_[last] >= bottom) {
      --last;
    }
    auto changed = false;
    for (auto i = first; i <= last; ++i) {
      auto const was_measured = blocks_[i].measured;
      auto const was_height = blocks_[i].height;
      measure(i);
      changed = changed || !was_measured || blocks_[i].height != was_height;
    }
    if (!changed) {
      return {first, last + 1};
    }
  }
}

auto text_layout::paragraph_of(std::size_t block) -> std::shared_ptr<paragraph const> {
  return measure(block);
}

auto text_layout::replace(std::size_t begin, std::size_t end, std::string_view replacement)
    -> void {
  end = std::min(end, text_.size());
  begin = std::min(begin, end);
  auto first = block_at_offset(begin);
  auto last = block_at_offset(end);
  // An edit that ends inside a line break joins the blocks on both sides of it.
  if (end > blocks_[last].end && last + 1 < blocks_.size()) {
    ++last;
  }
  auto const region_begin = blocks_[first].begin;
  auto const region_end = std::max(blocks_[last].end, end);

  auto region = text_.substr(region_begin, begin - region_begin);
  region += replacement;
  region += std::string_view{text_}.substr(end, region_end - end);
  text_.replace(region_begin, region_end - region_begin, region);

  auto replaced = split_blocks(region_begin, region);
  auto const delta = static_cast<std::ptrdiff_t>(region.size()) -
                     static_cast<std::ptrdiff_t>(region_end - region_begin);
  auto const old_count = last - first + 1;
  auto const new_count = replaced.size();
  auto const from = blocks_.begin() + static_cast<std::ptrdiff_t>(first);
  blocks_.erase(from, from + static_cast<std::ptrdiff_t>(old_count));
  blocks_.insert(blocks_.begin() + static_cast<std::ptrdiff_t>(first),
                 std::make_move_iterator(replaced.begin()),
                 std::make_move_iterator(replaced.end()));
  for (auto i = first + new_count; i < blocks_.size(); ++i) {
    blocks_[i].begin =
        static_cast<std::size_t>(static_cast<std::ptrdiff_t>(blocks_[i].begin) + delta);
    blocks_[i].end = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(blocks_[i].end) + delta);
  }

  tops_valid_ = std::min(tops_valid_, first);
  kept_.clear();
  cached_.clear();
  for (auto i = std::size_t{0}; i < blocks_.size(); ++i) {
    if (blocks_[i].laid_out) {
      cached_.push_back(i);
    }
  }
}

auto text_layout::hit_test(point position) -> text_hit {
  auto const index = block_at_y(position.y);
  auto const top = block_top(index);
  auto result = measure(index)->hit_test({position.x, position.y - top});
  result.offset += blocks_[index].begin;
  return result;
}

auto text_layout::caret_rect(std::size_t offset, caret_affinity affinity) -> rect {
  auto const index = block_at_offset(offset);
  auto const &item = blocks_[index];
  auto const local = std::min(offset - item.begin, item.end - item.begin);
  auto const result = measure(index)->caret_rect(local, affinity);
  return translated(result, {0, block_top(index)});
}

auto text_layout::selection_rects(std::size_t begin, std::size_t end, float top, float bottom)
    -> std::vector<rect> {
  end = std::min(end, text_.size());
  auto rects = std::vector<rect>{};
  if (begin >= end) {
    return rects;
  }
  auto const marker = options_.paragraph.shape.style.font_size * break_marker_em;
  auto const last = block_at_offset(end);
  for (auto index = block_at_offset(begin); index <= last; ++index) {
    auto const block_y = block_top(index);
    if (block_y >= bottom) {
      break;
    }
    if (block_y + blocks_[index].height <= top) {
      continue;
    }
    auto const &item = blocks_[index];
    auto const length = item.end - item.begin;
    auto const from = std::max(begin, item.begin) - item.begin;
    auto const to = std::min(std::min(end, item.end) - item.begin, length);
    auto const laid_out = measure(index);
    auto const offset = point{0, block_top(index)};
    if (from < to) {
      for (auto const &r : laid_out->selection_rects(from, to)) {
        rects.push_back(translated(r, offset));
      }
    }
    auto const breaks_after = index + 1 < blocks_.size();
    if (breaks_after && begin <= item.end && end > item.end) {
      auto const caret = laid_out->caret_rect(length, caret_affinity::upstream);
      auto const &tail = laid_out->lines().back();
      auto const left = tail.right_to_left() ? caret.left() - marker : caret.left();
      rects.push_back(translated({{left, caret.top()}, {marker, caret.height()}}, offset));
    }
  }
  return merge_touching_rects(std::move(rects));
}

} // namespace basalt::text
