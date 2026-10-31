#include <catch2/catch_test_macros.hpp>

#include "basalt/icon.h"
#include "basalt/widgets/box.h"
#include "basalt/widgets/button.h"

#include <algorithm>
#include <memory>

#include "text/font_support.h"
#include "ui_fixture.h"

using namespace basalt;
using namespace basalt::testing;
namespace a11y = basalt::accessibility;

namespace {

struct fixture {
  fixture() {
    root = &f.tree.emplace_root<box>(axis::horizontal);
    root->set_cross_alignment(alignment::start);
    root->set_main_alignment(alignment::start);
    target = &root->emplace_child<button>("Save", [this] { ++presses; });
    f.layout();
  }

  ui f;
  box *root = nullptr;
  button *target = nullptr;
  int presses = 0;
};

// The Latin text font and the icon font, as an application would register them.
struct icon_fixture {
  icon_fixture() {
    auto const &bundled = basalt::test::bundled_fonts();
    for (auto const &face : bundled.faces()) {
      fonts.add_file(face->path());
    }
    fonts.add_data(icon_font_data());
    f.tree.set_text_shaper(&shaper);
    root = &f.tree.emplace_root<box>(axis::horizontal);
    root->set_cross_alignment(alignment::start);
    root->set_main_alignment(alignment::start);
  }

  auto add(std::string label, icon i = icon::none) -> button & {
    auto &b = root->emplace_child<button>(std::move(label));
    b.set_icon(i);
    f.layout();
    return b;
  }

  // The families of the glyph runs painted, in paint order.
  auto painted_families() -> std::vector<std::string> {
    std::vector<std::string> families;
    for (auto const &c : f.commands()) {
      if (auto const *glyphs = std::get_if<draw_glyph_run_command>(&c)) {
        families.push_back(glyphs->run->face->family());
      }
    }
    return families;
  }

  text::font_manager fonts;
  text::text_shaper shaper{fonts};
  ui f;
  box *root = nullptr;
};

} // namespace

TEST_CASE("a button is at least the minimum control size", "[button]") {
  fixture f;
  auto const &sizes = f.f.tree.theme().sizes;
  REQUIRE(f.target->bounds().height() >= f.target->resolve(sizes.dimension_height_element_min));
  REQUIRE(f.target->bounds().width() >= f.target->resolve(sizes.dimension_width_element_min));
  REQUIRE(f.target->has_minimum_target());
}

TEST_CASE("releasing the pointer over a button presses it", "[button]") {
  fixture f;
  point const inside = f.target->bounds().center();
  f.f.send(press(inside));
  REQUIRE(f.presses == 0);
  f.f.send(release(inside));
  REQUIRE(f.presses == 1);
  REQUIRE(f.target->has_focus());
}

TEST_CASE("releasing away from the button cancels the press", "[button]") {
  fixture f;
  f.f.send(press(f.target->bounds().center()));
  f.f.send(move({300, 250}, pointer_button::primary));
  f.f.send(release({300, 250}));
  REQUIRE(f.presses == 0);
}

TEST_CASE("secondary clicks do not press", "[button]") {
  fixture f;
  point const inside = f.target->bounds().center();
  f.f.send(press(inside, pointer_button::secondary));
  f.f.send(release(inside, pointer_button::secondary));
  REQUIRE(f.presses == 0);
}

TEST_CASE("Enter and Space press a focused button, and Tab reaches it", "[button]") {
  fixture f;
  f.f.tap(key::tab);
  REQUIRE(f.target->has_focus());
  f.f.tap(key::enter);
  f.f.tap(key::space);
  REQUIRE(f.presses == 2);
  f.f.tap(key::enter, modifier::control);
  REQUIRE(f.presses == 2);
}

TEST_CASE("a disabled button keeps focus but does nothing", "[button]") {
  fixture f;
  f.target->set_enabled(false);
  f.target->request_focus();
  REQUIRE(f.target->has_focus());
  f.f.tap(key::enter);
  f.f.click(f.target->bounds().center());
  REQUIRE(f.presses == 0);
  REQUIRE_FALSE(f.target->press());
}

TEST_CASE("a button is a named button with press and focus actions", "[button][accessibility]") {
  fixture f;
  f.target->set_shortcut("Ctrl+S");
  auto const snapshot = f.f.semantics();
  auto const *node = snapshot.find(f.target->id());
  REQUIRE(node != nullptr);
  REQUIRE(node->semantics.kind == a11y::role::button);
  REQUIRE(node->semantics.name == "Save");
  REQUIRE(node->semantics.keyboard_shortcut == "Ctrl+S");
  REQUIRE(node->focusable);
  REQUIRE(node->supports(a11y::action::press));

  REQUIRE(a11y::perform_action(f.f.tree, f.target->id(), {a11y::action::press}));
  REQUIRE(f.presses == 1);
  REQUIRE(a11y::audit(snapshot).empty());
}

TEST_CASE("an accessible name replaces the label for assistive technology",
          "[button][accessibility]") {
  fixture f;
  f.target->set_accessible_name("Format: Save");
  auto const snapshot = f.f.semantics();
  auto const *node = snapshot.find(f.target->id());
  REQUIRE(node != nullptr);
  REQUIRE(node->semantics.name == "Format: Save");
  REQUIRE(f.target->label() == "Save");
  REQUIRE_FALSE(node->semantics.checked.has_value());
}

TEST_CASE("a toggled button reports checked or unchecked", "[button][accessibility]") {
  fixture f;
  f.target->set_toggled(true);
  {
    auto const snapshot = f.f.semantics();
    auto const *node = snapshot.find(f.target->id());
    REQUIRE(node != nullptr);
    REQUIRE(node->semantics.checked == a11y::check_state::checked);
  }
  f.target->set_toggled(false);
  {
    auto const snapshot = f.f.semantics();
    auto const *node = snapshot.find(f.target->id());
    REQUIRE(node != nullptr);
    REQUIRE(node->semantics.checked == a11y::check_state::unchecked);
  }
}

TEST_CASE("a button paints its fill by variant and state", "[button][paint]") {
  fixture f;
  auto const fill_of = [&] {
    for (auto const &c : f.f.commands()) {
      if (auto const *r = std::get_if<fill_rounded_rect_command>(&c)) {
        return r->color;
      }
    }
    return rgba{};
  };
  auto const &colors = f.f.tree.theme().colors;

  f.target->set_variant(button_variant::primary);
  REQUIRE(fill_of() == colors.background_accent);
  f.f.send(move(f.target->bounds().center()));
  REQUIRE(fill_of() == colors.background_accent_hover);
  f.f.send(press(f.target->bounds().center()));
  REQUIRE(fill_of() == colors.background_accent_active);
  f.f.send(release(f.target->bounds().center()));

  f.target->set_variant(button_variant::secondary);
  f.f.send(move({300, 250}));
  REQUIRE(fill_of() == rgba{});
  REQUIRE(count_commands<stroke_rounded_rect_command>(f.f.commands()) == 1);
}

TEST_CASE("an icon makes a button wider and removing it restores the width", "[button][icon]") {
  icon_fixture f;
  auto &b = f.add("Bold");
  float const plain = b.bounds().width();
  b.set_icon(icon::bold);
  f.f.layout();
  CHECK(b.icon() == icon::bold);
  CHECK(b.bounds().width() > plain);
  b.set_icon(icon::none);
  f.f.layout();
  CHECK(b.bounds().width() == plain);
}

TEST_CASE("an icon-only button is square and at least the minimum size", "[button][icon]") {
  icon_fixture f;
  auto &b = f.add("", icon::bold);
  auto const &sizes = f.f.tree.theme().sizes;
  CHECK(b.bounds().width() == b.bounds().height());
  CHECK(b.bounds().width() >= b.resolve(sizes.dimension_width_element_min));
  CHECK(b.bounds().height() >= b.resolve(sizes.dimension_height_element_min));
}

TEST_CASE("an icon-only button needs an accessible name", "[button][icon][a11y]") {
  icon_fixture f;
  auto &b = f.add("", icon::bold);
  auto const issues = a11y::audit(f.f.semantics());
  REQUIRE(issues.size() == 1);
  CHECK(issues.front().kind == a11y::issue_kind::missing_name);
  b.set_accessible_name("Bold");
  CHECK(a11y::audit(f.f.semantics()).empty());
  CHECK(b.accessibility_info().name == "Bold");
}

TEST_CASE("an icon is painted in the icon font and the label in the text font",
          "[button][icon][paint]") {
  icon_fixture f;
  f.add("Bold", icon::bold);
  auto const families = f.painted_families();
  REQUIRE(families.size() == 2);
  CHECK(families.front() == icon_font_family);
  CHECK(families.back() != icon_font_family);
}

TEST_CASE("an icon-only button paints one glyph in the icon font", "[button][icon][paint]") {
  icon_fixture f;
  f.add("", icon::bold);
  CHECK(f.painted_families() == std::vector<std::string>{std::string{icon_font_family}});
}

TEST_CASE("a button without an icon paints no icon glyph", "[button][icon][paint]") {
  icon_fixture f;
  f.add("Bold");
  auto const families = f.painted_families();
  CHECK(families.size() == 1);
  CHECK(std::ranges::count(families, std::string{icon_font_family}) == 0);
}
