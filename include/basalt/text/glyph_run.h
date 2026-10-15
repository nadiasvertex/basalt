#pragma once

#include "basalt/geometry.h"
#include "basalt/text/font_face.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace basalt::text {

// One shaped glyph. `position` is the glyph origin relative to the run origin
// (the baseline start), with y growing downward as everywhere in Basalt.
// `cluster` is the UTF-8 byte offset in the shaped text of the first character
// the glyph represents; glyphs of one cluster share it.
struct glyph {
  std::uint32_t id = 0;
  point position;
  float advance = 0;
  std::uint32_t cluster = 0;
};

// Glyphs of one face at one size in one direction, in display order from left
// to right. Layout pixels throughout. Immutable once shaped and shared with
// paint commands, so repainting does not copy glyphs.
struct glyph_run {
  std::shared_ptr<font_face const> face;
  float size = 0;
  std::vector<glyph> glyphs;
  // Distance from the run origin to where the next run starts.
  float advance = 0;
  // Bidi embedding level; odd levels run right to left.
  std::uint8_t level = 0;
  // The text bytes the run covers, [begin, end).
  std::size_t text_begin = 0;
  std::size_t text_end = 0;

  [[nodiscard]] auto right_to_left() const -> bool { return (level & 1U) != 0; }
};

using glyph_run_ptr = std::shared_ptr<glyph_run const>;

} // namespace basalt::text
