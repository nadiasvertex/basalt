#pragma once

#include "basalt/geometry.h"
#include "basalt/text/text_shaper.h"

#include <cstddef>
#include <functional>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace basalt::text {

enum class text_align {
  start, // The side the paragraph's base direction starts on.
  center,
  end,
};

// Which side of a line wrap a caret at that offset sits on. At a soft wrap the
// same offset is the end of one line and the start of the next.
enum class caret_affinity {
  upstream,   // Attached to the character before the offset.
  downstream, // Attached to the character after the offset.
};

// Returns the sorted offsets of every grapheme start in the text, followed by
// the text size. Callers with a full Unicode segmenter supply it through
// `paragraph_options::graphemes`.
using grapheme_segmenter = std::function<std::vector<std::size_t>(std::string_view)>;

// A built-in approximation of extended grapheme clusters: combining marks,
// joiners, emoji modifiers and ZWJ sequences, regional-indicator pairs, CRLF.
[[nodiscard]] auto default_grapheme_boundaries(std::string_view utf8) -> std::vector<std::size_t>;

// Combines rectangles on the same top that overlap or touch, sorted by top and
// then left. Selection geometry uses it so adjacent runs draw as one block.
[[nodiscard]] auto merge_touching_rects(std::vector<rect> rects) -> std::vector<rect>;

// Horizontal space reserved in a line for content the owner draws itself, such as a label. The
// space sits just before the character at `offset`, or at the end of the text when `offset` is the
// text size, and the character and everything after it move aside. Gaps at one offset are given
// as one gap with their combined width. A caret at the offset sits after the gap and a selection
// that starts there starts after it, so the owner's content is never selected. Offsets are
// relative to the paragraph text and should be grapheme boundaries.
struct inline_gap {
  std::size_t offset = 0;
  float width = 0;
};

struct paragraph_options {
  // Metrics are layout pixels. `style.line_height` is the distance between
  // baselines; zero uses the fonts' natural line height.
  shape_options shape;
  float max_width = std::numeric_limits<float>::infinity();
  bool wrap = true;
  text_align align = text_align::start;
  // Null uses `default_grapheme_boundaries`.
  grapheme_segmenter graphemes;
  // Sorted by offset, at most one per offset. They count toward the width of a line.
  std::vector<inline_gap> gaps;
  // Space added below every line, for content the owner draws between lines. It is part of the
  // line's height; the text sits at the top of the line.
  float line_padding = 0;
};

// One visual line. Offsets are UTF-8 byte offsets into the paragraph text.
// Everything is in paragraph coordinates, with the origin at the top left.
struct paragraph_line {
  std::size_t begin = 0;
  // Excludes the hard line break that ends the line, if any.
  std::size_t end = 0;
  // True when `end` is a soft wrap, so the next line begins at `end`.
  bool wrapped = false;
  float x = 0;
  float y = 0;
  // Advance of the whole line, including spaces that hang at a soft wrap.
  float width = 0;
  float height = 0;
  float baseline = 0; // Distance from `y` down to the baseline.
  float ascent = 0;
  float descent = 0;
  std::uint8_t base_level = 0;
  // Display order. `positioned_run::x` is relative to the paragraph origin,
  // and every `glyph::cluster` is relative to the paragraph text.
  std::vector<positioned_run> runs;

  [[nodiscard]] auto right_to_left() const -> bool { return (base_level & 1U) != 0; }
};

struct text_hit {
  std::size_t offset = 0;
  caret_affinity affinity = caret_affinity::downstream;
  std::size_t line = 0;
  // False when the point lies outside the line's box.
  bool inside = false;
};

// Laid-out text: shaped, broken into lines, and queryable for hit testing and
// caret and selection geometry. The text may contain hard line breaks (LF,
// CRLF and CR); each hard-broken part is its own bidi paragraph. Immutable
// once built.
//
// Lines break after spaces and hyphens and between ideographs (see
// `break_opportunities`); a word longer than the width breaks between
// graphemes. Spaces at a soft wrap hang past the edge rather than wrapping.
class paragraph {
public:
  paragraph(text_shaper const &shaper, std::string_view utf8, paragraph_options const &options);

  [[nodiscard]] auto text_size() const -> std::size_t { return text_size_; }
  [[nodiscard]] auto lines() const -> std::span<paragraph_line const> { return lines_; }
  // The widest line and the total height.
  [[nodiscard]] auto extent() const -> basalt::size { return extent_; }
  [[nodiscard]] auto grapheme_boundaries() const -> std::span<std::size_t const> {
    return graphemes_;
  }

  // The line holding `offset`, which is clamped to the text.
  [[nodiscard]] auto line_of(std::size_t offset,
                             caret_affinity affinity = caret_affinity::downstream) const
      -> std::size_t;
  // The line at paragraph y; points above or below clamp to the first or last.
  [[nodiscard]] auto line_at_y(float y) const -> std::size_t;

  // The caret position nearest `point`. Offsets are always grapheme boundaries.
  [[nodiscard]] auto hit_test(point position) const -> text_hit;
  // The same on a given line, for moving the caret up and down.
  [[nodiscard]] auto hit_test_line(std::size_t line, float x) const -> text_hit;

  // A zero-width rectangle as tall as the line. In bidi text an offset on a
  // direction change has two positions; affinity picks the one attached to
  // the character before or after it.
  [[nodiscard]] auto caret_rect(std::size_t offset,
                                caret_affinity affinity = caret_affinity::downstream) const -> rect;

  // The space reserved at `offset` by `paragraph_options::gaps`, as a rectangle as tall as the line
  // without its padding. Empty when there is no gap at the offset.
  [[nodiscard]] auto gap_rect(std::size_t offset) const -> std::optional<rect>;

  // Rectangles covering [begin, end) in visual order, merged where they touch.
  // A selection that continues past a hard line break gets a short extra
  // rectangle at the end of that line, so selected empty lines are visible.
  [[nodiscard]] auto selection_rects(std::size_t begin, std::size_t end) const -> std::vector<rect>;

private:
  struct stop {
    std::size_t offset = 0;
    float x = 0;
    bool leading = false;  // The edge before the character at `offset`.
    bool trailing = false; // The edge after the character before `offset`.
  };

  auto break_into_lines(text_shaper const &shaper, std::string_view utf8,
                        paragraph_options const &options) -> void;
  [[nodiscard]] auto line_stops(paragraph_line const &line) const -> std::vector<stop>;
  // Caret stops of one run, in the run's glyph order.
  auto append_run_stops(positioned_run const &placed, std::vector<stop> &out) const -> void;

  std::size_t text_size_ = 0;
  float break_marker_width_ = 0;
  float line_padding_ = 0;
  std::vector<inline_gap> gaps_;
  std::vector<std::size_t> graphemes_;
  std::vector<paragraph_line> lines_;
  basalt::size extent_;
};

} // namespace basalt::text
