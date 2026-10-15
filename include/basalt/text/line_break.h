#pragma once

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace basalt::text {

// True for spaces that may end a line and then hang past its edge. No-break
// spaces such as U+00A0 are excluded.
[[nodiscard]] auto is_breaking_space(char32_t scalar) -> bool;

// Byte offsets, strictly inside `utf8`, where a line may break. `graphemes` is
// the sorted list of grapheme start offsets followed by the text size; breaks
// only fall on those offsets.
//
// This is a small subset of UAX #14: after runs of breaking spaces and
// zero-width spaces, after hyphens inside words, and around ideographs, kana
// and Hangul except next to closing or opening punctuation. No-break spaces
// and joiners never break. Text must not contain hard line breaks.
[[nodiscard]] auto break_opportunities(std::string_view utf8,
                                       std::span<std::size_t const> graphemes)
    -> std::vector<std::size_t>;

} // namespace basalt::text
