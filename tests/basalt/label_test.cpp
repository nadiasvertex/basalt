#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "basalt/widgets/label.h"
#include "basalt/widgets/separator.h"

#include "../text/font_support.h"
#include "ui_fixture.h"

using namespace basalt;
using namespace basalt::testing;
namespace a11y = basalt::accessibility;

TEST_CASE("a label without a shaper measures one empty line", "[label]") {
  ui f;
  auto &root = f.tree.emplace_root<widget>();
  auto &text = root.emplace_child<label>("Hello");
  f.layout();
  basalt::size const wanted = text.measure({});
  REQUIRE(wanted.width == 0);
  REQUIRE(wanted.height ==
          text.resolve(relative(f.tree.theme().text.body_regular_medium.line_height)));
}

TEST_CASE("label text size follows the text scale", "[label][scaling]") {
  ui f({400, 300}, {1, 1, 2});
  auto &root = f.tree.emplace_root<widget>();
  auto &text = root.emplace_child<label>("Hello");
  f.layout();
  REQUIRE(text.measure({}).height == 2 * f.tree.theme().text.body_regular_medium.line_height);
}

TEST_CASE("labels are text semantics, headings carry a level", "[label][accessibility]") {
  ui f;
  auto &root = f.tree.emplace_root<widget>();
  auto &plain = root.emplace_child<label>("Plain");
  auto &heading = root.emplace_child<label>("Title", text_role::heading_large);
  heading.set_heading_level(1);
  f.layout();
  auto const snapshot = f.semantics();
  REQUIRE(snapshot.find(plain.id())->semantics.kind == a11y::role::label);
  auto const *node = snapshot.find(heading.id());
  REQUIRE(node->semantics.kind == a11y::role::heading);
  REQUIRE(node->semantics.level == 1);
  REQUIRE(node->semantics.name == "Title");
  REQUIRE(a11y::audit(snapshot).empty());
}

TEST_CASE("changing a label invalidates layout and paint", "[label]") {
  ui f;
  auto &root = f.tree.emplace_root<widget>();
  auto &text = root.emplace_child<label>("Hello");
  f.layout();
  text.set_bounds({{0, 0}, {50, 20}});
  f.layout();
  (void)f.tree.take_damage();
  text.set_text("Hello");
  REQUIRE_FALSE(text.needs_layout());
  text.set_text("Goodbye");
  REQUIRE(text.needs_layout());
  text.set_tone(text_tone::danger);
  REQUIRE_FALSE(f.tree.damage().empty());
}

TEST_CASE("tones map to theme text colors", "[label]") {
  theme const t = make_theme(color_scheme::dark);
  REQUIRE(text_color_for(t, text_tone::normal) == t.colors.text_default);
  REQUIRE(text_color_for(t, text_tone::subdued) == t.colors.text_subdued);
  REQUIRE(text_color_for(t, text_tone::danger) == t.colors.text_danger);
}

TEST_CASE("a shaped label draws its glyphs in the tone color", "[label][paint]") {
  auto &fonts = med::test::system_fonts();
  if (!med::test::has_coverage(fonts, U'A')) {
    SKIP("no font covers Latin letters");
  }
  text::text_shaper shaper{fonts};
  ui f;
  f.tree.set_text_shaper(&shaper);
  auto &root = f.tree.emplace_root<widget>();
  auto &text = root.emplace_child<label>("Hello");
  text.set_tone(text_tone::subdued);
  f.layout();
  REQUIRE(text.measure({}).width > 0);
  text.set_bounds({{10, 10}, text.measure({})});

  int runs = 0;
  for (auto const &c : f.commands()) {
    if (auto const *glyphs = std::get_if<draw_glyph_run_command>(&c)) {
      ++runs;
      REQUIRE(glyphs->color == f.tree.theme().colors.text_subdued);
      REQUIRE(glyphs->origin.y > 0);
    }
  }
  REQUIRE(runs >= 1);
}

TEST_CASE("a separator is a hairline across its parent", "[separator]") {
  ui f;
  auto &root = f.tree.emplace_root<widget>();
  auto &line = root.emplace_child<separator>();
  f.layout();
  basalt::size const wanted = line.measure({});
  REQUIRE(wanted.height == 9);
  line.set_bounds({{0, 0}, {100, wanted.height}});
  paint_commands commands = f.commands();
  REQUIRE(count_commands<fill_rect_command>(commands) == 1);
  auto const &fill = std::get<fill_rect_command>(*std::ranges::find_if(
      commands, [](auto const &c) { return std::holds_alternative<fill_rect_command>(c); }));
  REQUIRE(fill.bounds.width() == 100);
  REQUIRE(fill.bounds.height() == 1);

  line.set_direction(axis::vertical);
  REQUIRE(line.measure({}).width == 9);
  REQUIRE(a11y::build(f.tree).find(line.id())->semantics.orientation ==
          a11y::orientation::vertical);
}
