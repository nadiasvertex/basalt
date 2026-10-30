#include <catch2/catch_test_macros.hpp>

#include "basalt/widgets/box.h"
#include "basalt/widgets/button.h"

#include <memory>

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
