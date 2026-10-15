#pragma once

#include "basalt/text/bidi.h"
#include "basalt/text/font_manager.h"
#include "basalt/text/glyph_run.h"
#include "basalt/typography.h"

#include <cstddef>
#include <string_view>
#include <vector>

namespace basalt::text {

// A change of face for a byte range of the shaped text.
struct style_span {
  std::size_t begin = 0;
  std::size_t end = 0;
  bool bold = false;
  bool italic = false;
  // The size relative to the paragraph's font size.
  float scale = 1;
  // How far the baseline is raised, as a fraction of the paragraph's font size; negative lowers it.
  float rise = 0;
  // Shapes the characters of the range as the capitals `shape_options::capital` gives. The text
  // keeps its bytes.
  bool capitals = false;

  friend auto operator==(style_span const &, style_span const &) -> bool = default;
};

struct shape_options {
  // Metrics are layout pixels: apply `to_layout` before shaping.
  text_style style;
  bool italic = false;
  // Sorted, non-overlapping byte ranges that use the bold weight, the italic face, another size
  // and a raised or lowered baseline on top of `style` and `italic`. Text is shaped in pieces at
  // the span edges, so ligatures and kerning do not cross them. Ranges must not split a character.
  std::vector<style_span> spans;
  // The capital of a character, for spans with `capitals`. A capital that takes a different
  // number of UTF-8 bytes than the character is not used.
  char32_t (*capital)(char32_t) = nullptr;
  bidi_base direction = bidi_base::automatic;
  // BCP 47 tag such as "ja"; empty uses the process default.
  std::string_view language;
};

struct positioned_run {
  glyph_run_ptr run;
  // Distance from the start of the shaped text to the run origin.
  float x = 0;
};

// Shaped text as a line. Runs are in display order, each at its own `x`.
struct shaped_text {
  std::vector<positioned_run> runs;
  float width = 0;
  float ascent = 0;
  float descent = 0;
  float line_gap = 0;
  std::uint8_t base_level = 0;
};

// Shapes UTF-8 text with HarfBuzz. Text is split into bidi runs, then by
// script and by font, so every glyph run has one face, one script and one
// direction. Characters no font covers draw as the primary font's missing
// glyph.
//
// The text is one paragraph or line: it is not broken, and separators such as
// newline are shaped like any other character.
class text_shaper {
public:
  explicit text_shaper(font_manager &fonts) : fonts_(&fonts) {}

  [[nodiscard]] auto shape(std::string_view utf8, shape_options const &options) const
      -> shaped_text;

private:
  [[nodiscard]] auto shape_spans(std::string_view utf8, shape_options const &options) const
      -> shaped_text;

  font_manager *fonts_;
};

} // namespace basalt::text
