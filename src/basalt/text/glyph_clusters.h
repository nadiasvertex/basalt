#pragma once

#include "basalt/text/glyph_run.h"

#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>

namespace basalt::text::detail {

// How far a selected hard line break extends, as a fraction of the font size.
inline constexpr auto break_marker_em = 0.25F;

// The glyphs of the run grouped by cluster, in display order.
template <class Run> [[nodiscard]] auto clusters(Run &run) {
  return std::span{run.glyphs} | std::views::chunk_by([](glyph const &a, glyph const &b) {
           return a.cluster == b.cluster;
         });
}

// Moves the run from a part of the text to the whole text, which starts `offset` bytes earlier.
inline auto offset_clusters(glyph_run &run, std::size_t offset) -> void {
  for (auto &g : run.glyphs) {
    g.cluster += static_cast<std::uint32_t>(offset);
  }
  run.text_begin += offset;
  run.text_end += offset;
}

} // namespace basalt::text::detail
