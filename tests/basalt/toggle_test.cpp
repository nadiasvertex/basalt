#include <catch2/catch_test_macros.hpp>

#include "basalt/widgets/box.h"
#include "basalt/widgets/toggle.h"

#include <vector>

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
    target = &root->emplace_child<toggle>("Notifications");
    target->set_on_change([this](bool on) { changes.push_back(on); });
    f.layout();
  }

  ui f;
  box *root = nullptr;
  toggle *target = nullptr;
  std::vector<bool> changes;
};

} // namespace

TEST_CASE("a click flips a toggle and reports the new value", "[toggle]") {
  fixture f;
  REQUIRE_FALSE(f.target->on());
  f.f.click(f.target->bounds().center());
  REQUIRE(f.target->on());
  f.f.click(f.target->bounds().center());
  REQUIRE_FALSE(f.target->on());
  REQUIRE(f.changes == std::vector<bool>{true, false});
}

TEST_CASE("Space and Enter flip a focused toggle", "[toggle]") {
  fixture f;
  f.f.tap(key::tab);
  REQUIRE(f.target->has_focus());
  f.f.tap(key::space);
  f.f.tap(key::enter);
  REQUIRE(f.changes == std::vector<bool>{true, false});
}

TEST_CASE("setting the value from code does not call the handler", "[toggle]") {
  fixture f;
  f.target->set_on(true);
  REQUIRE(f.target->on());
  REQUIRE(f.changes.empty());
}

TEST_CASE("a disabled toggle does not change", "[toggle]") {
  fixture f;
  f.target->set_enabled(false);
  f.f.click(f.target->bounds().center());
  REQUIRE_FALSE(f.target->on());
  REQUIRE_FALSE(f.target->activate());
}

TEST_CASE("status text follows the value", "[toggle]") {
  text::text_shaper shaper{basalt::test::bundled_fonts()};
  ui f;
  f.tree.set_text_shaper(&shaper);
  auto &root = f.tree.emplace_root<box>(axis::horizontal);
  auto &target = root.emplace_child<toggle>("Notifications");
  f.layout();
  constexpr constraints wide{{}, {500, 100}};
  auto const without_status = target.measure(wide).width;

  target.set_status_text("Off", "Enabled");
  auto const off_width = target.measure(wide).width;
  REQUIRE(off_width > without_status);
  f.click(target.bounds().center());
  REQUIRE(target.on());
  REQUIRE(target.measure(wide).width > off_width);

  target.clear_status_text();
  REQUIRE(target.measure(wide).width == without_status);
}

TEST_CASE("a toggle is a named switch with a checked state", "[toggle][accessibility]") {
  fixture f;
  f.target->set_on(true);
  auto const snapshot = f.f.semantics();
  auto const *node = snapshot.find(f.target->id());
  REQUIRE(node != nullptr);
  REQUIRE(node->semantics.kind == a11y::role::switch_control);
  REQUIRE(node->semantics.name == "Notifications");
  REQUIRE(node->semantics.checked == a11y::check_state::checked);
  REQUIRE(node->supports(a11y::action::press));
  REQUIRE(a11y::audit(snapshot).empty());

  REQUIRE(a11y::perform_action(f.f.tree, f.target->id(), {a11y::action::press}));
  REQUIRE_FALSE(f.target->on());
}

TEST_CASE("a toggle paints the accent track when on", "[toggle][paint]") {
  fixture f;
  auto const first_fill = [&] {
    for (auto const &c : f.f.commands()) {
      if (auto const *r = std::get_if<fill_rounded_rect_command>(&c)) {
        return r->color;
      }
    }
    return rgba{};
  };
  auto const &colors = f.f.tree.theme().colors;
  REQUIRE(first_fill() == colors.background_secondary);
  f.target->set_on(true);
  REQUIRE(first_fill() == colors.background_accent);
}
