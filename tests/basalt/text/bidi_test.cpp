#include <catch2/catch_test_macros.hpp>

#include "basalt/text/bidi.h"

#include <string>

using namespace basalt::text;

namespace {

// "אב" in Hebrew and "اب" in Arabic.
constexpr auto hebrew = "\xD7\x90\xD7\x91";
constexpr auto arabic = "\xD8\xA7\xD8\xA8";

} // namespace

TEST_CASE("pure left-to-right text is one level-zero run", "[bidi]") {
  auto const paragraph = resolve_bidi("hello world");
  CHECK(paragraph.base_level == 0);
  REQUIRE(paragraph.runs.size() == 1);
  CHECK(paragraph.runs[0] == bidi_run{0, 11, 0});
}

TEST_CASE("every ascii character resolves to one level-zero run", "[bidi]") {
  auto text = std::string{};
  for (auto c = 0; c < 0x80; ++c) {
    text.push_back(static_cast<char>(c));
    text += "1 ";
  }
  for (auto const base : {bidi_base::automatic, bidi_base::left_to_right}) {
    auto const paragraph = resolve_bidi(text, base);
    CHECK(paragraph.base_level == 0);
    REQUIRE(paragraph.runs.size() == 1);
    CHECK(paragraph.runs[0] == bidi_run{0, text.size(), 0});
  }
}

TEST_CASE("ascii text in a right-to-left paragraph keeps its level", "[bidi]") {
  auto const paragraph = resolve_bidi("abc 123", bidi_base::right_to_left);
  CHECK(paragraph.base_level == 1);
  REQUIRE(paragraph.runs.size() == 1);
  CHECK(paragraph.runs[0] == bidi_run{0, 7, 2});
}

TEST_CASE("empty text has no runs", "[bidi]") {
  auto const paragraph = resolve_bidi("");
  CHECK(paragraph.runs.empty());
  CHECK(visual_order(paragraph.runs).empty());
}

TEST_CASE("the first strong character sets the paragraph direction", "[bidi]") {
  CHECK(resolve_bidi(hebrew).base_level == 1);
  CHECK(resolve_bidi(arabic).base_level == 1);
  CHECK(resolve_bidi("123 abc").base_level == 0);
  CHECK(resolve_bidi("  !?").base_level == 0);
  CHECK(resolve_bidi(std::string{"abc "} + hebrew).base_level == 0);
  CHECK(resolve_bidi(std::string{"  "} + hebrew).base_level == 1);
}

TEST_CASE("an explicit base direction overrides the text", "[bidi]") {
  CHECK(resolve_bidi("abc", bidi_base::right_to_left).base_level == 1);
  CHECK(resolve_bidi(hebrew, bidi_base::left_to_right).base_level == 0);
}

TEST_CASE("hebrew inside latin text becomes a level-one run", "[bidi]") {
  auto const text = std::string{"ab "} + hebrew + " cd";
  auto const paragraph = resolve_bidi(text);
  REQUIRE(paragraph.runs.size() == 3);
  CHECK(paragraph.runs[0] == bidi_run{0, 3, 0});
  CHECK(paragraph.runs[1] == bidi_run{3, 7, 1});
  CHECK(paragraph.runs[2] == bidi_run{7, 10, 0});
  CHECK(visual_order(paragraph.runs) == std::vector<std::size_t>{0, 1, 2});
}

TEST_CASE("neutrals between opposite directions take the paragraph direction", "[bidi]") {
  auto const text = std::string{"a"} + " " + hebrew;
  auto const paragraph = resolve_bidi(text);
  REQUIRE(paragraph.runs.size() == 2);
  CHECK(paragraph.runs[0] == bidi_run{0, 2, 0});
  CHECK(paragraph.runs[1].level == 1);
}

TEST_CASE("neutrals between two right-to-left letters join them", "[bidi]") {
  auto const text = std::string{hebrew} + " " + hebrew;
  auto const paragraph = resolve_bidi(text);
  REQUIRE(paragraph.runs.size() == 1);
  CHECK(paragraph.base_level == 1);
  CHECK(paragraph.runs[0].level == 1);
}

TEST_CASE("digits in right-to-left text sit one level above", "[bidi]") {
  auto const text = std::string{hebrew} + " 12";
  auto const paragraph = resolve_bidi(text);
  REQUIRE(paragraph.runs.size() == 2);
  CHECK(paragraph.runs[0].level == 1);
  CHECK(paragraph.runs[1].level == 2);
  CHECK(paragraph.runs[1].begin == 5);
  // The number reads left to right, after the Hebrew in logical order, so it
  // is displayed to the left.
  CHECK(visual_order(paragraph.runs) == std::vector<std::size_t>{1, 0});
}

TEST_CASE("digits after left-to-right letters stay left to right", "[bidi]") {
  auto const paragraph = resolve_bidi("abc 123");
  REQUIRE(paragraph.runs.size() == 1);
  CHECK(paragraph.runs[0].level == 0);
}

TEST_CASE("arabic digits form a number inside arabic text", "[bidi]") {
  auto const text = std::string{arabic} + " \xD9\xA1\xD9\xA2";
  auto const paragraph = resolve_bidi(text);
  REQUIRE(paragraph.runs.size() == 2);
  CHECK(paragraph.runs[1].level == 2);
}

TEST_CASE("european digits after an arabic letter become arabic numbers", "[bidi]") {
  auto const text = std::string{arabic} + "12";
  auto const paragraph = resolve_bidi(text);
  REQUIRE(paragraph.runs.size() == 2);
  CHECK(paragraph.runs[1].level == 2);
}

TEST_CASE("a separator between digits stays part of the number", "[bidi]") {
  auto const text = std::string{hebrew} + " 1,234";
  auto const paragraph = resolve_bidi(text);
  REQUIRE(paragraph.runs.size() == 2);
  CHECK(paragraph.runs[1] == bidi_run{5, 10, 2});
}

TEST_CASE("a currency sign next to a number joins the number", "[bidi]") {
  auto const text = std::string{hebrew} + " 5%";
  auto const paragraph = resolve_bidi(text);
  REQUIRE(paragraph.runs.size() == 2);
  CHECK(paragraph.runs[1].level == 2);
  CHECK(paragraph.runs[1].end == 7);
}

TEST_CASE("combining marks follow the character before them", "[bidi]") {
  auto const text = std::string{"\xD7\x90\xD6\xB0"}; // alef with sheva
  auto const paragraph = resolve_bidi(text);
  REQUIRE(paragraph.runs.size() == 1);
  CHECK(paragraph.runs[0].level == 1);
}

TEST_CASE("trailing whitespace takes the paragraph direction", "[bidi]") {
  auto const text = std::string{hebrew} + "   ";
  auto const paragraph = resolve_bidi(text, bidi_base::left_to_right);
  REQUIRE(paragraph.runs.size() == 2);
  CHECK(paragraph.runs[0].level == 1);
  CHECK(paragraph.runs[1] == bidi_run{4, 7, 0});
}

TEST_CASE("runs cover the text without gaps even with boundary neutrals", "[bidi]") {
  auto const text = std::string{"a\xE2\x80\x8B"} + hebrew + "\xE2\x80\x8B";
  auto const paragraph = resolve_bidi(text);
  auto expected_begin = std::size_t{0};
  for (auto const &run : paragraph.runs) {
    CHECK(run.begin == expected_begin);
    expected_begin = run.end;
  }
  CHECK(expected_begin == text.size());
}

TEST_CASE("invalid bytes are neutral and still covered", "[bidi]") {
  auto const text = std::string{"a\xFF\xFE"} + "b";
  auto const paragraph = resolve_bidi(text);
  REQUIRE(paragraph.runs.size() == 1);
  CHECK(paragraph.runs[0] == bidi_run{0, 4, 0});
}

TEST_CASE("visual order reverses nested levels", "[bidi]") {
  auto const runs = std::vector<bidi_run>{{0, 1, 0}, {1, 2, 1}, {2, 3, 2}, {3, 4, 1}, {4, 5, 0}};
  CHECK(visual_order(runs) == std::vector<std::size_t>{0, 3, 2, 1, 4});
}

TEST_CASE("visual order of an all right-to-left paragraph is reversed", "[bidi]") {
  auto const runs = std::vector<bidi_run>{{0, 1, 1}, {1, 2, 2}, {2, 3, 1}};
  CHECK(visual_order(runs) == std::vector<std::size_t>{2, 1, 0});
}
