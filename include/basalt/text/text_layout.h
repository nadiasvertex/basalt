#pragma once

#include "basalt/geometry.h"
#include "basalt/text/paragraph.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::text {

struct text_layout_options {
  // Used for every block unless `block_options` is set. `max_width` is
  // ignored: the layout's own width applies.
  paragraph_options paragraph;
  // Vertical space between blocks.
  float block_spacing = 0;
  // Chooses the options of one block, for example a heading style.
  std::function<paragraph_options(std::size_t block, std::string_view text)> block_options;
  // Space reserved below a block's text, for content the owner draws itself. It is part of the
  // block's height, so later blocks sit below it. It must give the same value for a block until
  // the owner calls `set_options` or `invalidate`, and it is called with the block index only.
  std::function<float(std::size_t block)> block_footer;
  // Laid-out paragraphs kept in memory. Blocks that are evicted keep their
  // measured height and are laid out again when next needed.
  std::size_t max_cached_paragraphs = 512;
};

// A document of blocks stacked vertically, laid out lazily so that only the
// blocks near the viewport are shaped. Text is split into blocks at LF and
// CRLF: a source file has one block per line, an article one per paragraph.
// Offsets are UTF-8 byte offsets into the whole text.
//
// Blocks that have not been measured use an estimated height, so
// `total_height` and block tops change as blocks are measured. Call
// `layout_range` for the visible span before reading tops. Queries that need
// a block measure it. Not thread-safe; queries are not const because they
// fill the paragraph cache.
class text_layout {
public:
  struct block_range {
    std::size_t first = 0;
    std::size_t last = 0; // One past the end.

    [[nodiscard]] auto empty() const -> bool { return first == last; }
  };

  // `shaper` must outlive the layout.
  text_layout(text_shaper const &shaper, std::string text, text_layout_options options = {});

  [[nodiscard]] auto text() const -> std::string_view { return text_; }
  [[nodiscard]] auto block_count() const -> std::size_t { return blocks_.size(); }
  [[nodiscard]] auto width() const -> float { return width_; }

  // Measurements are kept for the last few widths, so returning to a width that was used
  // recently, such as when a side panel opens and closes, finds its paragraphs in place.
  // Changing the options or the shaper state discards every measurement at every width.
  auto set_width(float width) -> void;
  auto set_options(text_layout_options options) -> void;
  // Call after fonts or the shaper's inputs change.
  auto invalidate() -> void;
  // Lays out one block again after its options changed, and keeps the other measurements.
  auto invalidate_block(std::size_t block) -> void;

  // Replaces [begin, end) with `replacement`. Blocks outside the edit keep
  // their measurements and shift with the text.
  auto replace(std::size_t begin, std::size_t end, std::string_view replacement) -> void;

  [[nodiscard]] auto total_height() -> float;

  // Measures the blocks that intersect [top, bottom) and returns them. Blocks
  // below a block whose height was estimated move when it is measured, so the
  // range is extended until the measured blocks stay put.
  auto layout_range(float top, float bottom) -> block_range;

  [[nodiscard]] auto block_begin(std::size_t block) const -> std::size_t;
  // Excludes the line break that separates the block from the next.
  [[nodiscard]] auto block_end(std::size_t block) const -> std::size_t;
  [[nodiscard]] auto block_top(std::size_t block) -> float;
  [[nodiscard]] auto block_height(std::size_t block) -> float;
  [[nodiscard]] auto is_measured(std::size_t block) const -> bool;
  [[nodiscard]] auto cached_paragraph_count() const -> std::size_t { return cached_.size(); }
  // How many other widths have measurements kept.
  [[nodiscard]] auto kept_width_count() const -> std::size_t { return kept_.size(); }

  [[nodiscard]] auto block_at_offset(std::size_t offset) const -> std::size_t;
  [[nodiscard]] auto block_at_y(float y) -> std::size_t;

  // The laid-out block. Offsets inside it are relative to `block_begin`.
  [[nodiscard]] auto paragraph_of(std::size_t block) -> std::shared_ptr<paragraph const>;

  // The nearest caret position to `position`, in layout coordinates, with a
  // document offset.
  [[nodiscard]] auto hit_test(point position) -> text_hit;
  // A zero-width rectangle as tall as the line holding the offset.
  [[nodiscard]] auto caret_rect(std::size_t offset,
                                caret_affinity affinity = caret_affinity::downstream) -> rect;
  // Selection rectangles for [begin, end), limited to blocks that intersect
  // [top, bottom) so a huge selection only measures what is visible.
  [[nodiscard]] auto selection_rects(std::size_t begin, std::size_t end,
                                     float top = -std::numeric_limits<float>::infinity(),
                                     float bottom = std::numeric_limits<float>::infinity())
      -> std::vector<rect>;

private:
  struct block {
    std::size_t begin = 0;
    std::size_t end = 0;
    float height = 0;
    bool measured = false;
    std::uint64_t last_used = 0;
    std::shared_ptr<paragraph const> laid_out;
  };

  // What one width has measured, set aside while another width is in use.
  struct block_measure {
    float height = 0;
    bool measured = false;
    std::uint64_t last_used = 0;
    std::shared_ptr<paragraph const> laid_out;
  };
  struct kept_width {
    float width = 0;
    std::vector<block_measure> blocks;
    std::vector<std::size_t> cached;
  };
  static constexpr std::size_t max_kept_widths = 2;

  auto split_blocks(std::size_t base, std::string_view text) const -> std::vector<block>;
  // Drops the measurements at the current width only.
  auto clear_measurements() -> void;
  // Drops the measurements at every width.
  auto reset_measurements() -> void;
  auto keep_current_width() -> void;
  auto restore_width(kept_width &kept) -> void;
  auto ensure_tops(std::size_t upto) -> void;
  auto measure(std::size_t index) -> std::shared_ptr<paragraph const>;
  [[nodiscard]] auto estimate_height(std::size_t index) const -> float;
  [[nodiscard]] auto footer_of(std::size_t index) const -> float;
  [[nodiscard]] auto options_for(std::size_t index) const -> paragraph_options;
  auto evict() -> void;

  text_shaper const *shaper_;
  std::string text_;
  text_layout_options options_;
  float width_ = std::numeric_limits<float>::infinity();
  std::vector<block> blocks_;
  // Top of each block; valid for indices below `tops_valid_`.
  std::vector<float> tops_;
  std::size_t tops_valid_ = 0;
  std::vector<std::size_t> cached_;
  std::vector<kept_width> kept_;
  std::uint64_t clock_ = 0;
};

} // namespace basalt::text
