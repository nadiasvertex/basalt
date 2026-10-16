#include <catch2/catch_test_macros.hpp>

#include "basalt/text/font_manager.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

#include "font_support.h"

using namespace basalt::text;

TEST_CASE("an empty manager matches nothing", "[fonts]") {
  auto fonts = font_manager{};
  CHECK(fonts.faces().empty());
  CHECK(fonts.match({"Noto Sans"}) == nullptr);
  CHECK(fonts.primary_face({"Noto Sans"}) == nullptr);
  CHECK(fonts.fallback_for(U'A', {"Noto Sans"}) == nullptr);
  CHECK(fonts.families().empty());
}

TEST_CASE("files that are not fonts are skipped", "[fonts]") {
  auto const directory = std::filesystem::temp_directory_path() / "basalt_font_manager_test";
  std::filesystem::create_directories(directory);
  auto const file = directory / "not_a_font.ttf";
  std::ofstream{file} << "this is plain text, not an OpenType font";

  auto fonts = font_manager{};
  CHECK(fonts.add_file(file) == 0);
  CHECK(fonts.add_file(directory / "missing.ttf") == 0);
  CHECK(fonts.add_directory(directory) == 0);
  CHECK(fonts.add_data(nullptr) == 0);
  CHECK(fonts.add_data(std::make_shared<std::vector<std::byte> const>(8)) == 0);
  CHECK(fonts.faces().empty());
  std::filesystem::remove_all(directory);
}

TEST_CASE("a missing directory adds nothing", "[fonts]") {
  auto fonts = font_manager{};
  CHECK(fonts.add_directory("/nonexistent/basalt/fonts") == 0);
}

TEST_CASE("system fonts load with names, weights and metrics", "[fonts]") {
  auto &fonts = basalt::test::system_fonts();
  if (fonts.faces().empty()) {
    SKIP("no system fonts");
  }
  for (auto const &face : fonts.faces()) {
    CHECK_FALSE(face->family().empty());
    CHECK(face->weight() >= 100);
    CHECK(face->weight() <= 900);
    CHECK(face->units_per_em() > 0);
    CHECK_FALSE(face->data().empty());
  }
  auto const ids = [&] {
    auto result = std::vector<std::uint64_t>{};
    for (auto const &face : fonts.faces()) {
      result.push_back(face->id());
    }
    std::ranges::sort(result);
    return result;
  }();
  CHECK(std::ranges::adjacent_find(ids) == ids.end());
}

TEST_CASE("a face with latin coverage reports glyphs and metrics", "[fonts]") {
  auto &fonts = basalt::test::bundled_fonts();
  auto const face = fonts.fallback_for(U'A', {"Noto Sans"});
  REQUIRE(face != nullptr);
  CHECK(face->covers(U'A'));
  CHECK(face->glyph_for(U'A').value_or(0) != 0);
  auto const metrics = face->metrics(100);
  CHECK(metrics.ascent > 0);
  CHECK(metrics.descent >= 0);
  CHECK(metrics.height() >= 100 * 0.5F);
  auto const doubled = face->metrics(200);
  CHECK(doubled.ascent == metrics.ascent * 2);
}

namespace {

auto outline_bounds(glyph_outline const &outline) -> basalt::rect {
  auto bounds = basalt::rect{outline.points.front(), {}};
  for (auto const point : outline.points) {
    bounds = basalt::rect::from_edges(
        std::min(bounds.left(), point.x), std::min(bounds.top(), point.y),
        std::max(bounds.right(), point.x), std::max(bounds.bottom(), point.y));
  }
  return bounds;
}

auto points_used(glyph_outline const &outline) -> std::size_t {
  auto count = std::size_t{0};
  for (auto const verb : outline.verbs) {
    count += verb == outline_verb::quad_to    ? 2
             : verb == outline_verb::cubic_to ? 3
             : verb == outline_verb::close    ? 0
                                              : 1;
  }
  return count;
}

} // namespace

TEST_CASE("a glyph outline stands on the baseline at the requested size", "[fonts]") {
  auto &fonts = basalt::test::bundled_fonts();
  auto const face = fonts.match({"Noto Sans"});
  REQUIRE(face != nullptr);
  auto const glyph = face->glyph_for(U'H');
  REQUIRE(glyph.has_value());

  auto const outline = face->outline(*glyph, 100);
  REQUIRE_FALSE(outline.empty());
  CHECK(outline.verbs.front() == outline_verb::move_to);
  CHECK(outline.verbs.back() == outline_verb::close);
  CHECK(points_used(outline) == outline.points.size());
  auto const bounds = outline_bounds(outline);
  CHECK(bounds.left() >= 0);
  CHECK(bounds.right() <= 100);
  // Capitals rise above the baseline, which is y = 0, and stop on it.
  CHECK(bounds.top() < -60);
  CHECK(bounds.top() > -100);
  CHECK(bounds.bottom() == 0);

  auto const doubled = face->outline(*glyph, 200);
  REQUIRE(doubled.points.size() == outline.points.size());
  CHECK(doubled.points[1].x == outline.points[1].x * 2);
  CHECK(doubled.points[1].y == outline.points[1].y * 2);
}

TEST_CASE("a space has no outline", "[fonts]") {
  auto &fonts = basalt::test::bundled_fonts();
  auto const face = fonts.match({"Noto Sans"});
  REQUIRE(face != nullptr);
  CHECK(face->outline(face->glyph_for(U' ').value_or(0), 100).empty());
}

TEST_CASE("a variable face draws its outlines at its own weight", "[fonts]") {
  auto &fonts = basalt::test::bundled_fonts();
  auto const regular = fonts.match({"Noto Sans", 400});
  auto const bold = fonts.match({"Noto Sans", 700});
  REQUIRE(regular != nullptr);
  REQUIRE(bold != nullptr);
  REQUIRE(bold->variation_weight() == 700);
  REQUIRE(regular->variation_weight() == 400);
  auto const stem = regular->glyph_for(U'l');
  REQUIRE(stem.has_value());
  CHECK(outline_bounds(bold->outline(*stem, 100)).width() >
        outline_bounds(regular->outline(*stem, 100)).width() * 1.3F);
}

TEST_CASE("matching is case-insensitive and picks the nearest weight", "[fonts]") {
  auto &fonts = basalt::test::system_fonts();
  auto const families = fonts.families();
  if (families.empty()) {
    SKIP("no system fonts");
  }
  auto const &family = families.front();
  auto upper = family;
  std::ranges::transform(upper, upper.begin(),
                         [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

  auto const regular = fonts.match({family, 400});
  REQUIRE(regular != nullptr);
  CHECK(fonts.match({upper, 400}) == regular);

  auto const bold = fonts.match({family, 900});
  REQUIRE(bold != nullptr);
  CHECK(bold->weight() >= regular->weight());
  CHECK(bold->family() == regular->family());
  CHECK(fonts.match({"No Such Family"}) == nullptr);
}

TEST_CASE("fallback prefers the requested family when it has the glyph", "[fonts]") {
  auto &fonts = basalt::test::bundled_fonts();
  auto const requested = fonts.primary_face({"Noto Sans"});
  REQUIRE(requested != nullptr);
  REQUIRE(requested->covers(U'A'));
  auto const request = font_request{requested->family(), requested->weight(), requested->italic()};
  CHECK(fonts.fallback_for(U'A', request)->family() == requested->family());
  // The answer is cached and stable.
  CHECK(fonts.fallback_for(U'A', request) == fonts.fallback_for(U'A', request));
}

TEST_CASE("fallback finds a font for scripts the primary lacks", "[fonts]") {
  auto &fonts = basalt::test::system_fonts();
  auto const primary = fonts.primary_face({"Noto Sans"});
  if (primary == nullptr) {
    SKIP("no system fonts");
  }
  for (auto const scalar : {U'א', U'ا', U'あ', U'ก'}) {
    if (primary->covers(scalar)) {
      continue;
    }
    auto const fallback = fonts.fallback_for(scalar, {primary->family()});
    if (fallback == nullptr) {
      continue;
    }
    CHECK(fallback->covers(scalar));
  }
}

TEST_CASE("fallback for a base and a mark covers both", "[fonts]") {
  auto &fonts = basalt::test::bundled_fonts();
  auto const face = fonts.fallback_for(U"e\u0301", {"Noto Sans"});
  REQUIRE(face != nullptr);
  CHECK(face->covers(U'e'));
  CHECK(face->covers(U'\u0301'));
}
