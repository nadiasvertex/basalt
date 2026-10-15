#include <catch2/catch_test_macros.hpp>

#include "basalt/text/text_shaper.h"

#include <algorithm>

#include "font_support.h"

using namespace basalt;
using namespace basalt::text;

namespace {

constexpr auto hebrew = "\xD7\x90\xD7\x91\xD7\x92"; // alef bet gimel
constexpr auto arabic = "\xD8\xA7\xD9\x84\xD8\xB9"; // alef lam ain

auto options(float size = 20) -> shape_options {
  auto result = shape_options{};
  result.style = text_style{"Noto Sans", size};
  return result;
}

auto require_latin() -> font_manager & { return basalt::test::bundled_fonts(); }

auto glyph_count(shaped_text const &shaped) -> std::size_t {
  auto count = std::size_t{0};
  for (auto const &placed : shaped.runs) {
    count += placed.run->glyphs.size();
  }
  return count;
}

} // namespace

TEST_CASE("empty text shapes to no runs but keeps line metrics", "[shaper]") {
  auto &fonts = require_latin();
  auto const shaped = text_shaper{fonts}.shape("", options());
  CHECK(shaped.runs.empty());
  CHECK(shaped.width == 0);
  CHECK(shaped.ascent > 0);
}

TEST_CASE("with no fonts nothing is shaped", "[shaper]") {
  auto fonts = font_manager{};
  auto const shaped = text_shaper{fonts}.shape("abc", options());
  CHECK(shaped.runs.empty());
  CHECK(shaped.width == 0);
}

TEST_CASE("latin text shapes into one run with byte clusters", "[shaper]") {
  auto &fonts = require_latin();
  auto const shaped = text_shaper{fonts}.shape("Hello", options());
  REQUIRE(shaped.runs.size() == 1);
  auto const &run = *shaped.runs[0].run;
  CHECK(shaped.runs[0].x == 0);
  CHECK(run.level == 0);
  CHECK(run.size == 20);
  CHECK(run.text_begin == 0);
  CHECK(run.text_end == 5);
  REQUIRE(run.glyphs.size() == 5);
  for (auto i = std::size_t{0}; i < 5; ++i) {
    CHECK(run.glyphs[i].cluster == i);
    CHECK(run.glyphs[i].id != 0);
  }
  CHECK(run.advance > 0);
  CHECK(shaped.width == run.advance);
  CHECK(run.glyphs[1].position.x > run.glyphs[0].position.x);
}

TEST_CASE("run advance is the sum of glyph advances", "[shaper]") {
  auto &fonts = require_latin();
  auto const run = text_shaper{fonts}.shape("Hello world", options()).runs.at(0).run;
  auto sum = 0.0F;
  for (auto const &glyph : run->glyphs) {
    sum += glyph.advance;
  }
  CHECK(run->advance == sum);
}

TEST_CASE("text width scales with the font size", "[shaper]") {
  auto &fonts = require_latin();
  auto const small = text_shaper{fonts}.shape("Scaling", options(10));
  auto const large = text_shaper{fonts}.shape("Scaling", options(40));
  CHECK(large.width > small.width * 3.5F);
  CHECK(large.width < small.width * 4.5F);
  CHECK(large.ascent > small.ascent * 3.5F);
}

TEST_CASE("letter spacing widens every glyph", "[shaper]") {
  auto &fonts = require_latin();
  auto plain = options();
  auto spaced = options();
  spaced.style.letter_spacing = 3;
  auto const a = text_shaper{fonts}.shape("abcd", plain);
  auto const b = text_shaper{fonts}.shape("abcd", spaced);
  CHECK(b.width >= a.width + 4 * 3 - 0.01F);
}

TEST_CASE("combining marks share a cluster with their base", "[shaper]") {
  auto &fonts = require_latin();
  // "e" followed by a combining acute accent.
  auto const shaped = text_shaper{fonts}.shape("e\xCC\x81x", options());
  REQUIRE_FALSE(shaped.runs.empty());
  // The accent stays in the base letter's cluster and font.
  for (auto const &placed : shaped.runs) {
    for (auto const &glyph : placed.run->glyphs) {
      CHECK((glyph.cluster == 0 || glyph.cluster == 3));
    }
    CHECK_FALSE((placed.run->text_begin == 1 || placed.run->text_begin == 2));
  }
  CHECK(shaped.runs.front().run->glyphs.front().cluster == 0);
  CHECK(shaped.runs.back().run->glyphs.back().cluster == 3);
}

TEST_CASE("invalid utf-8 still produces glyphs", "[shaper]") {
  auto &fonts = require_latin();
  auto const shaped = text_shaper{fonts}.shape("a\xFF"
                                               "b",
                                               options());
  CHECK(glyph_count(shaped) >= 3);
  CHECK(shaped.width > 0);
}

TEST_CASE("each run covers one script and the runs cover the text", "[shaper]") {
  auto &fonts = basalt::test::system_fonts();
  if (!basalt::test::has_coverage(fonts, U'A') || !basalt::test::has_coverage(fonts, U'א')) {
    SKIP("fonts for Latin and Hebrew are missing");
  }
  auto const text = std::string{"abc "} + hebrew + " def";
  auto const shaped = text_shaper{fonts}.shape(text, options());
  REQUIRE(shaped.runs.size() >= 3);
  auto covered = std::vector<bool>(text.size());
  for (auto const &placed : shaped.runs) {
    for (auto i = placed.run->text_begin; i < placed.run->text_end; ++i) {
      CHECK_FALSE(covered[i]);
      covered[i] = true;
    }
  }
  CHECK(std::ranges::all_of(covered, [](bool c) { return c; }));
}

TEST_CASE("hebrew runs are right to left with display-ordered glyphs", "[shaper]") {
  auto &fonts = basalt::test::system_fonts();
  if (!basalt::test::has_coverage(fonts, U'א')) {
    SKIP("no font covers Hebrew");
  }
  auto const shaped = text_shaper{fonts}.shape(hebrew, options());
  CHECK(shaped.base_level == 1);
  REQUIRE_FALSE(shaped.runs.empty());
  auto const &run = *shaped.runs[0].run;
  CHECK(run.right_to_left());
  REQUIRE(run.glyphs.size() == 3);
  // Display order runs left to right, so the logically last letter is first.
  CHECK(run.glyphs[0].cluster > run.glyphs[1].cluster);
  CHECK(run.glyphs[1].cluster > run.glyphs[2].cluster);
  CHECK(run.glyphs[0].position.x < run.glyphs[2].position.x);
}

TEST_CASE("mixed text places runs in display order", "[shaper]") {
  auto &fonts = basalt::test::system_fonts();
  if (!basalt::test::has_coverage(fonts, U'A') || !basalt::test::has_coverage(fonts, U'א')) {
    SKIP("fonts for Latin and Hebrew are missing");
  }
  auto const text = std::string{"ab "} + hebrew;
  auto const shaped = text_shaper{fonts}.shape(text, options());
  auto previous_end = 0.0F;
  for (auto const &placed : shaped.runs) {
    CHECK(placed.x == previous_end);
    previous_end = placed.x + placed.run->advance;
  }
  CHECK(shaped.width == previous_end);
  CHECK(shaped.runs.front().run->text_begin == 0);
  CHECK(shaped.runs.back().run->right_to_left());
}

TEST_CASE("arabic letters are joined by shaping", "[shaper]") {
  auto &fonts = basalt::test::system_fonts();
  if (!basalt::test::has_coverage(fonts, U'ا')) {
    SKIP("no font covers Arabic");
  }
  auto const shaped = text_shaper{fonts}.shape(arabic, options());
  REQUIRE_FALSE(shaped.runs.empty());
  auto const &run = *shaped.runs[0].run;
  CHECK(run.right_to_left());
  // Contextual forms differ from the isolated codepoint glyphs.
  auto const isolated = run.face->glyph_for(U'ا');
  auto const final_form_differs =
      std::ranges::any_of(run.glyphs, [&](glyph const &g) { return g.id != isolated; });
  CHECK(final_form_differs);
}

TEST_CASE("characters outside every font use the missing glyph", "[shaper]") {
  auto &system = require_latin();
  auto const source = system.fallback_for(U'A', {"Noto Sans"});
  if (source->path().empty() || source->covers(U'\U0010FFFF')) {
    SKIP("the Latin font cannot show a missing glyph");
  }
  auto fonts = font_manager{};
  REQUIRE(fonts.add_file(source->path()) > 0);
  auto const shaped = text_shaper{fonts}.shape("\xF4\x8F\xBF\xBF", options());
  REQUIRE(shaped.runs.size() == 1);
  REQUIRE(shaped.runs[0].run->glyphs.size() == 1);
  CHECK(shaped.runs[0].run->glyphs[0].id == 0);
}

TEST_CASE("an explicit direction overrides the text", "[shaper]") {
  auto &fonts = require_latin();
  auto rtl = options();
  rtl.direction = bidi_base::right_to_left;
  auto const shaped = text_shaper{fonts}.shape("abc", rtl);
  CHECK(shaped.base_level == 1);
}

TEST_CASE("style spans shape pieces with byte clusters and total the piece widths", "[shaper]") {
  auto &fonts = require_latin();
  auto const shaper = text_shaper{fonts};
  auto styled = options();
  styled.spans.push_back({.begin = 2, .end = 4, .bold = true, .italic = true});
  auto const shaped = shaper.shape("abcdef", styled);

  REQUIRE(shaped.runs.size() == 3);
  CHECK(shaped.runs[0].run->text_begin == 0);
  CHECK(shaped.runs[1].run->text_begin == 2);
  CHECK(shaped.runs[1].run->text_end == 4);
  CHECK(shaped.runs[2].run->text_begin == 4);
  CHECK(shaped.runs[1].run->glyphs.front().cluster == 2);
  CHECK(shaped.runs[2].x == shaped.runs[1].x + shaped.runs[1].run->advance);
  CHECK(shaped.width == shaped.runs[2].x + shaped.runs[2].run->advance);

  auto const plain = shaper.shape("abcdef", options());
  CHECK(plain.runs.size() == 1);
}

TEST_CASE("style spans scale a piece and raise its glyphs", "[shaper]") {
  auto &fonts = require_latin();
  auto const shaper = text_shaper{fonts};
  auto styled = options();
  styled.spans.push_back({.begin = 2, .end = 4, .scale = 0.5F, .rise = 0.25F});
  auto const shaped = shaper.shape("abcdef", styled);

  REQUIRE(shaped.runs.size() == 3);
  auto const &small = *shaped.runs[1].run;
  CHECK(small.size == styled.style.font_size * 0.5F);
  CHECK(small.glyphs.front().position.y == -0.25F * styled.style.font_size);
  CHECK(shaped.runs[0].run->glyphs.front().position.y == 0);
  CHECK(shaped.ascent >= shaped.runs[0].run->face->metrics(shaped.runs[0].run->size).ascent);
}

TEST_CASE("capitals shape lowercase letters as capitals without changing the bytes", "[shaper]") {
  auto &fonts = require_latin();
  auto const shaper = text_shaper{fonts};
  auto styled = options();
  styled.spans.push_back({.begin = 0, .end = 3, .capitals = true});
  styled.capital = [](char32_t c) -> char32_t { return c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c; };
  auto const capitals = shaper.shape("abc", styled);
  auto const upper = shaper.shape("ABC", options());
  auto const lower = shaper.shape("abc", options());

  CHECK(capitals.width == upper.width);
  CHECK(capitals.width != lower.width);
  CHECK(capitals.runs.front().run->text_end == 3);
}
