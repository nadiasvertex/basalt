#include <catch2/catch_test_macros.hpp>

#include "basalt/icon.h"
#include "basalt/text/font_manager.h"

#include <algorithm>
#include <set>
#include <string>

using namespace basalt;

TEST_CASE("every icon has its own name and codepoint", "[icon]") {
  REQUIRE_FALSE(all_icons().empty());
  std::set<char32_t> codepoints;
  std::set<std::string> names;
  for (auto const i : all_icons()) {
    CHECK(i != icon::none);
    CHECK(icon_codepoint(i) >= 0xE000);
    CHECK(icon_codepoint(i) <= 0xF8FF);
    CHECK_FALSE(icon_name(i).empty());
    codepoints.insert(icon_codepoint(i));
    names.emplace(icon_name(i));
  }
  CHECK(codepoints.size() == all_icons().size());
  CHECK(names.size() == all_icons().size());
}

TEST_CASE("none has no codepoint and no name", "[icon]") {
  CHECK(icon_codepoint(icon::none) == U'\0');
  CHECK(icon_name(icon::none).empty());
  CHECK(std::ranges::find(all_icons(), icon::none) == all_icons().end());
}

TEST_CASE("icon_text is the codepoint in UTF-8", "[icon]") {
  CHECK(icon_text(icon::bold) == "\xEE\x96\xBE");
  CHECK(icon_text(icon::none).empty());
  for (auto const i : all_icons()) {
    CHECK(icon_text(i).size() == 3);
  }
}

TEST_CASE("an icon name converts back to the icon", "[icon]") {
  for (auto const i : all_icons()) {
    CHECK(icon_from_name(icon_name(i)) == i);
  }
  CHECK(icon_from_name("bold") == icon::bold);
}

TEST_CASE("an unknown icon name gives none", "[icon]") {
  CHECK(icon_from_name("") == icon::none);
  CHECK(icon_from_name("no-such-icon") == icon::none);
  CHECK(icon_from_name("Bold") == icon::none);
}

TEST_CASE("the bundled font loads and draws every icon", "[icon]") {
  text::font_manager fonts;
  REQUIRE(fonts.add_data(icon_font_data()) >= 1);
  CHECK(fonts.families() == std::vector<std::string>{std::string{icon_font_family}});
  auto const face = fonts.match({.family = std::string{icon_font_family}});
  REQUIRE(face != nullptr);
  for (auto const i : all_icons()) {
    INFO(icon_name(i));
    CHECK(face->covers(icon_codepoint(i)));
  }
}

TEST_CASE("the icon font covers nothing but the icons", "[icon]") {
  text::font_manager fonts;
  REQUIRE(fonts.add_data(icon_font_data()) >= 1);
  auto const face = fonts.match({.family = std::string{icon_font_family}});
  REQUIRE(face != nullptr);
  CHECK_FALSE(face->covers(U'A'));
  CHECK_FALSE(face->covers(U'\xE000'));
}
