#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "basalt/text/paragraph.h"

#include <algorithm>
#include <string>

#include "font_support.h"

using namespace basalt;
using namespace basalt::text;
using Catch::Matchers::WithinAbs;

namespace {

constexpr auto hebrew = "\xD7\x90\xD7\x91\xD7\x92"; // alef bet gimel

auto options(float width = std::numeric_limits<float>::infinity()) -> paragraph_options {
  auto result = paragraph_options{};
  result.shape.style = text_style{"Noto Sans", 20};
  result.max_width = width;
  return result;
}

auto require_latin() -> font_manager & {
  auto &fonts = med::test::system_fonts();
  if (!med::test::has_coverage(fonts, U'A')) {
    SKIP("no font covers Latin letters");
  }
  return fonts;
}

auto width_of(font_manager &fonts, std::string_view text) -> float {
  return text_shaper{fonts}.shape(text, options().shape).width;
}

auto make(font_manager &fonts, std::string_view text, paragraph_options const &opts) -> paragraph {
  return paragraph{text_shaper{fonts}, text, opts};
}

} // namespace

TEST_CASE("empty text is one empty line with metrics", "[paragraph]") {
  auto &fonts = require_latin();
  auto const p = make(fonts, "", options());
  REQUIRE(p.lines().size() == 1);
  CHECK(p.lines()[0].runs.empty());
  CHECK(p.lines()[0].begin == 0);
  CHECK(p.lines()[0].end == 0);
  CHECK(p.extent().height > 0);
  auto const caret = p.caret_rect(0);
  CHECK(caret.left() == 0);
  CHECK(caret.height() == p.lines()[0].height);
}

TEST_CASE("one line without a width limit", "[paragraph]") {
  auto &fonts = require_latin();
  auto const text = std::string{"The quick brown fox jumps over the lazy dog"};
  auto const p = make(fonts, text, options());
  REQUIRE(p.lines().size() == 1);
  CHECK(p.lines()[0].begin == 0);
  CHECK(p.lines()[0].end == text.size());
  CHECK_FALSE(p.lines()[0].wrapped);
  CHECK_THAT(p.extent().width, WithinAbs(width_of(fonts, text), 0.01));
}

TEST_CASE("hard breaks start new lines", "[paragraph]") {
  auto &fonts = require_latin();
  auto const p = make(fonts, "a\nb\n", options());
  REQUIRE(p.lines().size() == 3);
  CHECK(p.lines()[0].begin == 0);
  CHECK(p.lines()[0].end == 1);
  CHECK(p.lines()[1].begin == 2);
  CHECK(p.lines()[1].end == 3);
  CHECK(p.lines()[2].begin == 4);
  CHECK(p.lines()[2].end == 4);
  CHECK(p.lines()[1].y == p.lines()[0].height);
  CHECK(p.lines()[2].y == p.lines()[0].height + p.lines()[1].height);
  CHECK_FALSE(p.lines()[0].wrapped);
  CHECK(p.extent().height == p.lines()[2].y + p.lines()[2].height);
}

TEST_CASE("crlf and lone cr are single hard breaks", "[paragraph]") {
  auto &fonts = require_latin();
  auto const crlf = make(fonts, "a\r\nb", options());
  REQUIRE(crlf.lines().size() == 2);
  CHECK(crlf.lines()[0].end == 1);
  CHECK(crlf.lines()[1].begin == 3);
  auto const cr = make(fonts, "a\rb", options());
  REQUIRE(cr.lines().size() == 2);
  CHECK(cr.lines()[1].begin == 2);
}

TEST_CASE("text wraps at spaces within the width", "[paragraph]") {
  auto &fonts = require_latin();
  auto const text = std::string{"aaaa bbbb cccc dddd eeee"};
  auto const limit = width_of(fonts, "aaaa bbbb") + 2;
  auto const p = make(fonts, text, options(limit));
  REQUIRE(p.lines().size() > 1);
  for (auto i = std::size_t{0}; i < p.lines().size(); ++i) {
    auto const &line = p.lines()[i];
    auto trimmed = text.substr(line.begin, line.end - line.begin);
    while (trimmed.ends_with(' ')) {
      trimmed.pop_back();
    }
    CHECK(width_of(fonts, trimmed) <= limit + 0.1F);
    if (i + 1 < p.lines().size()) {
      CHECK(line.wrapped);
      CHECK(line.end == p.lines()[i + 1].begin);
      CHECK(text[p.lines()[i + 1].begin] != ' ');
    }
  }
  CHECK(p.lines().front().begin == 0);
  CHECK(p.lines().back().end == text.size());
  CHECK_FALSE(p.lines().back().wrapped);
}

TEST_CASE("wrapping off keeps one line", "[paragraph]") {
  auto &fonts = require_latin();
  auto opts = options(10);
  opts.wrap = false;
  CHECK(make(fonts, "aaaa bbbb cccc", opts).lines().size() == 1);
}

TEST_CASE("a word wider than the line breaks between graphemes", "[paragraph]") {
  auto &fonts = require_latin();
  auto const text = std::string{"abcdefghijklmnopqrstuvwxyz"};
  auto const p = make(fonts, text, options(width_of(fonts, text) / 3));
  REQUIRE(p.lines().size() >= 3);
  for (auto const &line : p.lines()) {
    CHECK(line.end > line.begin);
  }
  CHECK(p.lines().back().end == text.size());
}

TEST_CASE("an absurdly narrow width still makes progress", "[paragraph]") {
  auto &fonts = require_latin();
  auto const p = make(fonts, "abc", options(0.5F));
  REQUIRE(p.lines().size() == 3);
  CHECK(p.lines()[2].end == 3);
}

TEST_CASE("wrapping never splits a grapheme", "[paragraph]") {
  auto &fonts = require_latin();
  auto text = std::string{};
  for (auto i = 0; i < 12; ++i) {
    text += "e\xCC\x81";
  }
  auto const p = make(fonts, text, options(width_of(fonts, text) / 4));
  REQUIRE(p.lines().size() > 1);
  for (auto const &line : p.lines()) {
    CHECK(line.begin % 3 == 0);
    CHECK(line.end % 3 == 0);
  }
}

TEST_CASE("line runs use paragraph offsets and positions", "[paragraph]") {
  auto &fonts = require_latin();
  auto const text = std::string{"aaaa bbbb cccc"};
  auto const p = make(fonts, text, options(width_of(fonts, "aaaa bbbb") + 2));
  REQUIRE(p.lines().size() == 2);
  auto const &second = p.lines()[1];
  REQUIRE_FALSE(second.runs.empty());
  for (auto const &placed : second.runs) {
    CHECK(placed.run->text_begin >= second.begin);
    CHECK(placed.run->text_end <= second.end);
    for (auto const &g : placed.run->glyphs) {
      CHECK(g.cluster >= second.begin);
      CHECK(g.cluster < second.end);
    }
  }
  CHECK(second.runs.front().x == second.x);
}

TEST_CASE("line height and baseline", "[paragraph]") {
  auto &fonts = require_latin();
  auto natural = make(fonts, "x", options());
  auto const &line = natural.lines()[0];
  CHECK(line.height >= line.ascent + line.descent - 0.01F);
  CHECK(line.baseline >= line.ascent - 0.01F);

  auto tall = options();
  tall.shape.style.line_height = 50;
  auto const p = make(fonts, "x\ny", tall);
  CHECK(p.lines()[0].height == 50);
  CHECK(p.lines()[1].y == 50);
  CHECK_THAT(p.lines()[0].baseline,
             WithinAbs((50 - (line.ascent + line.descent)) / 2 + line.ascent, 0.5));
}

TEST_CASE("alignment places the line in the available width", "[paragraph]") {
  auto &fonts = require_latin();
  auto const w = width_of(fonts, "abc");
  auto opts = options(500);
  CHECK(make(fonts, "abc", opts).lines()[0].x == 0);
  opts.align = text_align::end;
  CHECK_THAT(make(fonts, "abc", opts).lines()[0].x, WithinAbs(500 - w, 0.01));
  opts.align = text_align::center;
  CHECK_THAT(make(fonts, "abc", opts).lines()[0].x, WithinAbs((500 - w) / 2, 0.01));
}

TEST_CASE("start alignment follows the base direction", "[paragraph]") {
  auto &fonts = med::test::system_fonts();
  if (!med::test::has_coverage(fonts, U'א')) {
    SKIP("no font covers Hebrew");
  }
  auto const p = make(fonts, hebrew, options(500));
  REQUIRE(p.lines().size() == 1);
  CHECK(p.lines()[0].right_to_left());
  CHECK_THAT(p.lines()[0].x + p.lines()[0].width, WithinAbs(500, 0.01));
}

TEST_CASE("caret positions grow along a left-to-right line", "[paragraph]") {
  auto &fonts = require_latin();
  auto const text = std::string{"Hello"};
  auto const p = make(fonts, text, options());
  CHECK(p.caret_rect(0).left() == 0);
  for (auto i = std::size_t{0}; i < text.size(); ++i) {
    CHECK(p.caret_rect(i + 1).left() > p.caret_rect(i).left());
  }
  CHECK_THAT(p.caret_rect(5).left(), WithinAbs(width_of(fonts, text), 0.01));
  CHECK_THAT(p.caret_rect(2).left(), WithinAbs(width_of(fonts, "He"), 0.5));
  CHECK(p.caret_rect(2).width() == 0);
  CHECK(p.caret_rect(2).top() == 0);
  CHECK(p.caret_rect(2).height() == p.lines()[0].height);
  // Offsets past the end clamp.
  CHECK(p.caret_rect(99).left() == p.caret_rect(5).left());
}

TEST_CASE("hit testing returns the nearest caret position", "[paragraph]") {
  auto &fonts = require_latin();
  auto const p = make(fonts, "Hello", options());
  for (auto i = std::size_t{0}; i <= 5; ++i) {
    auto const hit = p.hit_test({p.caret_rect(i).left(), 5});
    CHECK(hit.offset == i);
    CHECK(hit.line == 0);
  }
  CHECK(p.hit_test({-30, 5}).offset == 0);
  CHECK_FALSE(p.hit_test({-30, 5}).inside);
  CHECK(p.hit_test({1000, 5}).offset == 5);
  CHECK(p.hit_test({1000, 5}).affinity == caret_affinity::upstream);
  CHECK(p.hit_test({1, 5}).inside);
  // Halfway through a letter rounds to the closer side.
  auto const a = p.caret_rect(1).left();
  auto const b = p.caret_rect(2).left();
  CHECK(p.hit_test({a + (b - a) * 0.3F, 5}).offset == 1);
  CHECK(p.hit_test({a + (b - a) * 0.7F, 5}).offset == 2);
}

TEST_CASE("hit testing picks the line by height", "[paragraph]") {
  auto &fonts = require_latin();
  auto const p = make(fonts, "abc\ndef\nghi", options());
  REQUIRE(p.lines().size() == 3);
  auto const middle = p.lines()[1].y + p.lines()[1].height / 2;
  CHECK(p.hit_test({0, middle}).offset == 4);
  CHECK(p.hit_test({0, middle}).line == 1);
  CHECK(p.hit_test({1000, middle}).offset == 7);
  CHECK(p.hit_test({0, -50}).line == 0);
  CHECK(p.hit_test({0, 5000}).line == 2);
  CHECK_FALSE(p.hit_test({0, 5000}).inside);
  CHECK(p.hit_test_line(2, 0).offset == 8);
}

TEST_CASE("affinity chooses a side of a soft wrap", "[paragraph]") {
  auto &fonts = require_latin();
  auto const text = std::string{"aaaa bbbb cccc"};
  auto const p = make(fonts, text, options(width_of(fonts, "aaaa bbbb") + 2));
  REQUIRE(p.lines().size() == 2);
  auto const wrap = p.lines()[0].end;
  CHECK(p.lines()[1].begin == wrap);
  CHECK(p.line_of(wrap, caret_affinity::upstream) == 0);
  CHECK(p.line_of(wrap, caret_affinity::downstream) == 1);
  auto const before = p.caret_rect(wrap, caret_affinity::upstream);
  auto const after = p.caret_rect(wrap, caret_affinity::downstream);
  CHECK(before.top() == p.lines()[0].y);
  CHECK(after.top() == p.lines()[1].y);
  CHECK(after.left() == p.lines()[1].x);
  // A hard break has no such ambiguity.
  auto const hard = make(fonts, "ab\ncd", options());
  CHECK(hard.line_of(2, caret_affinity::upstream) == 0);
  CHECK(hard.line_of(3, caret_affinity::upstream) == 1);
}

TEST_CASE("carets and hits skip positions inside a grapheme", "[paragraph]") {
  auto &fonts = require_latin();
  // "e" + combining acute, "x".
  auto const p = make(fonts, "e\xCC\x81x", options());
  CHECK(p.grapheme_boundaries().size() == 3);
  CHECK(p.caret_rect(1).left() == p.caret_rect(0).left());
  CHECK(p.caret_rect(2).left() == p.caret_rect(0).left());
  CHECK(p.caret_rect(3).left() > 0);
  for (auto x = -5.0F; x < p.extent().width + 5; x += 1.5F) {
    auto const offset = p.hit_test({x, 3}).offset;
    CHECK((offset == 0 || offset == 3 || offset == 4));
  }
}

TEST_CASE("a supplied segmenter replaces the built-in one", "[paragraph]") {
  auto &fonts = require_latin();
  auto opts = options();
  opts.graphemes = [](std::string_view text) {
    auto result = std::vector<std::size_t>{};
    for (auto i = std::size_t{0}; i <= text.size(); i += 2) {
      result.push_back(i);
    }
    if (result.back() != text.size()) {
      result.push_back(text.size());
    }
    return result;
  };
  auto const p = make(fonts, "abcd", opts);
  CHECK(p.grapheme_boundaries().size() == 3);
  for (auto x = 0.0F; x < 100; x += 2) {
    CHECK(p.hit_test({x, 3}).offset % 2 == 0);
  }
}

TEST_CASE("an invalid segmenter result falls back to the built-in one", "[paragraph]") {
  auto &fonts = require_latin();
  auto opts = options();
  opts.graphemes = [](std::string_view) { return std::vector<std::size_t>{5, 2}; };
  CHECK(make(fonts, "abc", opts).grapheme_boundaries().size() == 4);
}

TEST_CASE("selection rectangles cover the selected text", "[paragraph]") {
  auto &fonts = require_latin();
  auto const p = make(fonts, "Hello world", options());
  auto const rects = p.selection_rects(0, 5);
  REQUIRE(rects.size() == 1);
  CHECK_THAT(rects[0].left(), WithinAbs(0, 0.01));
  CHECK_THAT(rects[0].right(), WithinAbs(p.caret_rect(5).left(), 0.01));
  CHECK(rects[0].height() == p.lines()[0].height);

  auto const middle = p.selection_rects(2, 8);
  REQUIRE(middle.size() == 1);
  CHECK_THAT(middle[0].left(), WithinAbs(p.caret_rect(2).left(), 0.01));
  CHECK_THAT(middle[0].right(), WithinAbs(p.caret_rect(8).left(), 0.01));

  CHECK(p.selection_rects(3, 3).empty());
  CHECK(p.selection_rects(5, 2).empty());
  auto const clamped = p.selection_rects(0, 500);
  REQUIRE(clamped.size() == 1);
  CHECK_THAT(clamped[0].right(), WithinAbs(p.extent().width, 0.01));
}

TEST_CASE("selections across wrapped lines get one rectangle per line", "[paragraph]") {
  auto &fonts = require_latin();
  auto const text = std::string{"aaaa bbbb cccc dddd"};
  auto const p = make(fonts, text, options(width_of(fonts, "aaaa bbbb") + 2));
  REQUIRE(p.lines().size() >= 2);
  auto const rects = p.selection_rects(0, text.size());
  CHECK(rects.size() == p.lines().size());
  for (auto i = std::size_t{0}; i < rects.size(); ++i) {
    CHECK(rects[i].top() == p.lines()[i].y);
  }
}

TEST_CASE("a selected hard break and empty lines are visible", "[paragraph]") {
  auto &fonts = require_latin();
  auto const p = make(fonts, "a\n\nb", options());
  REQUIRE(p.lines().size() == 3);
  auto const rects = p.selection_rects(0, 3);
  REQUIRE(rects.size() == 2);
  CHECK(rects[0].top() == p.lines()[0].y);
  CHECK(rects[0].right() > p.caret_rect(1).left());
  CHECK(rects[1].top() == p.lines()[1].y);
  CHECK(rects[1].width() > 0);
  // A selection that stops before the break does not include its marker.
  auto const inner = p.selection_rects(0, 1);
  REQUIRE(inner.size() == 1);
  CHECK_THAT(inner[0].right(), WithinAbs(p.caret_rect(1).left(), 0.01));
}

TEST_CASE("right-to-left text puts the start of the text on the right", "[paragraph]") {
  auto &fonts = med::test::system_fonts();
  if (!med::test::has_coverage(fonts, U'א')) {
    SKIP("no font covers Hebrew");
  }
  auto const p = make(fonts, hebrew, options());
  REQUIRE(p.lines().size() == 1);
  // Three letters of two bytes each.
  CHECK(p.caret_rect(0).left() > p.caret_rect(2).left());
  CHECK(p.caret_rect(2).left() > p.caret_rect(4).left());
  CHECK(p.caret_rect(4).left() > p.caret_rect(6).left());
  CHECK_THAT(p.caret_rect(6).left(), WithinAbs(0, 0.01));
  CHECK(p.hit_test({p.extent().width - 1, 3}).offset == 0);
  CHECK(p.hit_test({1, 3}).offset == 6);
  for (auto i = std::size_t{0}; i <= 6; i += 2) {
    CHECK(p.hit_test({p.caret_rect(i).left(), 3}).offset == i);
  }
  auto const rects = p.selection_rects(0, 2);
  REQUIRE(rects.size() == 1);
  CHECK_THAT(rects[0].right(), WithinAbs(p.caret_rect(0).left(), 0.01));
  CHECK_THAT(rects[0].left(), WithinAbs(p.caret_rect(2).left(), 0.01));
}

TEST_CASE("mixed direction selection covers each run", "[paragraph]") {
  auto &fonts = med::test::system_fonts();
  if (!med::test::has_coverage(fonts, U'A') || !med::test::has_coverage(fonts, U'א')) {
    SKIP("fonts for Latin and Hebrew are missing");
  }
  auto const text = std::string{"ab "} + hebrew + " cd";
  auto const p = make(fonts, text, options());
  REQUIRE(p.lines().size() == 1);
  // The whole line selected is one rectangle as wide as the line.
  auto const all = p.selection_rects(0, text.size());
  REQUIRE(all.size() == 1);
  CHECK_THAT(all[0].width(), WithinAbs(p.extent().width, 0.5));
  // Selecting only the Hebrew letters is one rectangle inside the line.
  auto const letters = p.selection_rects(3, 9);
  REQUIRE(letters.size() == 1);
  CHECK(letters[0].width() > 0);
  CHECK(letters[0].right() <= p.extent().width + 0.01F);
  // Every grapheme boundary round-trips through hit testing.
  for (auto i = std::size_t{0}; i <= text.size(); ++i) {
    if (!std::ranges::binary_search(p.grapheme_boundaries(), i)) {
      continue;
    }
    auto const caret = p.caret_rect(i);
    auto const hit = p.hit_test({caret.left(), 3});
    CHECK_THAT(p.caret_rect(hit.offset, hit.affinity).left(), WithinAbs(caret.left(), 0.01));
  }
}

TEST_CASE("a gap widens the line and moves the text after it", "[paragraph][gap]") {
  auto &fonts = require_latin();
  auto const plain = make(fonts, "Hello world", options());
  auto opts = options();
  opts.gaps = {{6, 30.0F}};
  auto const gapped = make(fonts, "Hello world", opts);
  CHECK_THAT(gapped.lines()[0].width, WithinAbs(plain.lines()[0].width + 30.0F, 0.01));

  // The caret before the gap's character sits after the gap; the text before it does not move.
  CHECK_THAT(gapped.caret_rect(0).left(), WithinAbs(0.0, 0.01));
  CHECK_THAT(gapped.caret_rect(6).left(), WithinAbs(plain.caret_rect(6).left() + 30.0F, 0.01));
  CHECK_THAT(gapped.caret_rect(11).left(), WithinAbs(plain.caret_rect(11).left() + 30.0F, 0.01));

  auto const gap = gapped.gap_rect(6);
  REQUIRE(gap);
  CHECK_THAT(gap->left(), WithinAbs(plain.caret_rect(6).left(), 0.01));
  CHECK_THAT(gap->width(), WithinAbs(30.0, 0.01));
  CHECK_FALSE(gapped.gap_rect(5));

  // A selection that starts at the gap does not include it, and one across it does.
  auto const from_gap = gapped.selection_rects(6, 11);
  REQUIRE(from_gap.size() == 1);
  CHECK_THAT(from_gap[0].left(), WithinAbs(plain.caret_rect(6).left() + 30.0F, 0.01));
  auto const across = gapped.selection_rects(0, 11);
  REQUIRE(across.size() == 1);
  CHECK_THAT(across[0].width(), WithinAbs(plain.lines()[0].width + 30.0F, 0.01));
}

TEST_CASE("a gap at the start or end of the text takes room", "[paragraph][gap]") {
  auto &fonts = require_latin();
  auto const plain = make(fonts, "Hello", options());
  auto opts = options();
  opts.gaps = {{0, 12.0F}, {5, 8.0F}};
  auto const gapped = make(fonts, "Hello", opts);
  CHECK_THAT(gapped.lines()[0].width, WithinAbs(plain.lines()[0].width + 20.0F, 0.01));
  CHECK_THAT(gapped.caret_rect(0).left(), WithinAbs(12.0, 0.01));
  auto const start = gapped.gap_rect(0);
  REQUIRE(start);
  CHECK_THAT(start->left(), WithinAbs(0.0, 0.01));
  auto const end = gapped.gap_rect(5);
  REQUIRE(end);
  CHECK_THAT(end->left(), WithinAbs(12.0 + plain.lines()[0].width, 0.01));

  auto const empty_opts = [] {
    auto o = options();
    o.gaps = {{0, 9.0F}};
    return o;
  }();
  auto const empty = make(fonts, "", empty_opts);
  CHECK_THAT(empty.lines()[0].width, WithinAbs(9.0, 0.01));
  CHECK_THAT(empty.caret_rect(0).left(), WithinAbs(9.0, 0.01));
}

TEST_CASE("gaps count toward line breaking", "[paragraph][gap]") {
  auto &fonts = require_latin();
  auto const text = std::string{"aaaa bbbb cccc"};
  auto const word = width_of(fonts, "aaaa bbbb");
  auto const fits = make(fonts, text, options(word + 1));
  auto opts = options(word + 1);
  opts.gaps = {{5, word}};
  auto const pushed = make(fonts, text, opts);
  CHECK(pushed.lines().size() > fits.lines().size());
  // The caret of every offset stays on the line that holds it.
  for (auto const &line : pushed.lines()) {
    CHECK(line.width <= word + 1 + 0.01F + word);
  }
}

TEST_CASE("line padding adds room below the text of every line", "[paragraph][gap]") {
  auto &fonts = require_latin();
  auto const plain = make(fonts, "one two three four five six seven", options(80));
  auto opts = options(80);
  opts.line_padding = 7.0F;
  auto const padded = make(fonts, "one two three four five six seven", opts);
  REQUIRE(padded.lines().size() == plain.lines().size());
  CHECK(plain.lines().size() > 1);
  for (std::size_t i = 0; i < plain.lines().size(); ++i) {
    CHECK_THAT(padded.lines()[i].height, WithinAbs(plain.lines()[i].height + 7.0F, 0.01));
    CHECK_THAT(padded.lines()[i].baseline, WithinAbs(plain.lines()[i].baseline, 0.01));
  }
  auto const gap_free = padded.gap_rect(0);
  CHECK_FALSE(gap_free);
}
