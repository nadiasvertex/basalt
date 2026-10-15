#include <catch2/catch_test_macros.hpp>

#include "basalt/text/line_break.h"
#include "basalt/text/paragraph.h"

#include <string>

using namespace basalt::text;

namespace {

auto breaks_of(std::string const &text) -> std::vector<std::size_t> {
  return break_opportunities(text, default_grapheme_boundaries(text));
}

using offsets = std::vector<std::size_t>;

} // namespace

TEST_CASE("breaks fall after spaces, not before them", "[line_break]") {
  CHECK(breaks_of("hello world") == offsets{6});
  CHECK(breaks_of("a b  c") == offsets{2, 5});
  CHECK(breaks_of("trailing ") == offsets{});
  CHECK(breaks_of("") == offsets{});
  CHECK(breaks_of("word") == offsets{});
}

TEST_CASE("no-break spaces and word joiners do not break", "[line_break]") {
  CHECK(breaks_of("a\xC2\xA0"
                  "b") == offsets{});
  CHECK(breaks_of("a\xE2\x81\xA0"
                  "b") == offsets{});
}

TEST_CASE("zero-width space allows a break after it", "[line_break]") {
  CHECK(breaks_of("ab\xE2\x80\x8B"
                  "cd") == offsets{5});
}

TEST_CASE("hyphens break inside words but not before digits", "[line_break]") {
  CHECK(breaks_of("well-known") == offsets{5});
  CHECK(breaks_of("x-5") == offsets{});
  CHECK(breaks_of("-a") == offsets{});
}

TEST_CASE("ideographs break between characters", "[line_break]") {
  // Three ideographs.
  CHECK(breaks_of("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E") == offsets{3, 6});
  // A closing ideographic full stop stays with the character before it.
  CHECK(breaks_of("\xE6\x97\xA5\xE6\x9C\xAC\xE3\x80\x82") == offsets{3});
  // An opening corner bracket stays with the character after it.
  CHECK(breaks_of("\xE6\x97\xA5\xE3\x80\x8C\xE6\x9C\xAC") == offsets{3});
}

TEST_CASE("breaks only fall on grapheme boundaries", "[line_break]") {
  // "e" with a combining acute, then a space and "x": the mark never separates.
  CHECK(breaks_of("e\xCC\x81 x") == offsets{4});
}

TEST_CASE("default graphemes join marks, ZWJ sequences, flags and CRLF", "[line_break]") {
  CHECK(default_grapheme_boundaries("") == offsets{0});
  CHECK(default_grapheme_boundaries("ab") == offsets{0, 1, 2});
  CHECK(default_grapheme_boundaries("e\xCC\x81x") == offsets{0, 3, 4});
  CHECK(default_grapheme_boundaries("\r\n") == offsets{0, 2});
  // Man ZWJ woman is one cluster.
  CHECK(default_grapheme_boundaries("\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9") ==
        offsets{0, 11});
  // Two flags are two clusters of two regional indicators each.
  CHECK(default_grapheme_boundaries("\xF0\x9F\x87\xBA\xF0\x9F\x87\xB8"
                                    "\xF0\x9F\x87\xAB\xF0\x9F\x87\xB7") == offsets{0, 8, 16});
  // Emoji with a skin-tone modifier.
  CHECK(default_grapheme_boundaries("\xF0\x9F\x91\x8D\xF0\x9F\x8F\xBD") == offsets{0, 8});
}

TEST_CASE("invalid utf-8 bytes are one grapheme each", "[line_break]") {
  CHECK(default_grapheme_boundaries("a\xFF\xFE") == offsets{0, 1, 2, 3});
}
