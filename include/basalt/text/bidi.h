#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace basalt::text {

enum class bidi_base {
  automatic, // The first strong character decides; left-to-right if there is none.
  left_to_right,
  right_to_left,
};

// A maximal stretch of text at one embedding level. Offsets are UTF-8 byte
// offsets into the resolved text.
struct bidi_run {
  std::size_t begin = 0;
  std::size_t end = 0;
  std::uint8_t level = 0;

  [[nodiscard]] constexpr auto right_to_left() const -> bool { return (level & 1U) != 0; }

  [[nodiscard]] friend constexpr auto operator==(bidi_run const &, bidi_run const &)
      -> bool = default;
};

struct bidi_paragraph {
  std::uint8_t base_level = 0;
  std::vector<bidi_run> runs; // Logical order, covering the whole text.
};

// Resolves embedding levels for one paragraph (or one line) of UTF-8 text with
// the Unicode Bidirectional Algorithm's weak, neutral and implicit rules (UAX
// #9 W1-W7, N1-N2, I1-I2, L1). Explicit embeddings, overrides and isolates are
// treated as neutral and bracket pairs get no special handling. Line and
// paragraph separators end the text of interest; pass one paragraph at a time.
[[nodiscard]] auto resolve_bidi(std::string_view utf8, bidi_base base = bidi_base::automatic)
    -> bidi_paragraph;

// Indices into `runs` in left-to-right display order (UAX #9 L2).
[[nodiscard]] auto visual_order(std::vector<bidi_run> const &runs) -> std::vector<std::size_t>;

} // namespace basalt::text
