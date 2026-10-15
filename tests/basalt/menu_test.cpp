#include <catch2/catch_test_macros.hpp>

#include "basalt/widgets/menu.h"
#include "basalt/widgets/stack.h"

#include <memory>
#include <string>
#include <vector>

#include "ui_fixture.h"

using namespace basalt;
using namespace basalt::testing;
namespace a11y = basalt::accessibility;

namespace {

struct fixture {
  fixture() {
    auto &root = f.tree.emplace_root<stack>();
    popup = &root.emplace_child<menu>("File");
    open = &popup->add_item("file.open", "Open", "Ctrl+O");
    popup->add_separator();
    save = &popup->add_item("file.save", "Save", "Ctrl+S");
    wrap = &popup->add_item("view.wrap", "Word wrap", {}, true);
    popup->set_on_activate([this](std::string const &id) { activated.push_back(id); });
    popup->set_on_dismiss([this] { ++dismissed; });
    f.layout();
  }

  ui f;
  menu *popup = nullptr;
  menu_item *open = nullptr;
  menu_item *save = nullptr;
  menu_item *wrap = nullptr;
  std::vector<std::string> activated;
  int dismissed = 0;
};

} // namespace

TEST_CASE("menu items stack and share the menu width", "[menu]") {
  fixture f;
  REQUIRE(f.open->bounds().top() < f.save->bounds().top());
  REQUIRE(f.open->bounds().width() == f.save->bounds().width());
  REQUIRE(f.open->bounds().height() >=
          f.open->resolve(f.f.tree.theme().sizes.dimension_height_element_min));
}

TEST_CASE("arrow keys move among items and wrap", "[menu]") {
  fixture f;
  f.f.tap(key::tab);
  REQUIRE(f.open->has_focus());
  f.f.tap(key::down);
  REQUIRE(f.save->has_focus());
  f.f.tap(key::down);
  REQUIRE(f.wrap->has_focus());
  f.f.tap(key::down);
  REQUIRE(f.open->has_focus());
  f.f.tap(key::up);
  REQUIRE(f.wrap->has_focus());
  f.f.tap(key::home);
  REQUIRE(f.open->has_focus());
  f.f.tap(key::end);
  REQUIRE(f.wrap->has_focus());
}

TEST_CASE("the menu is one Tab stop", "[menu]") {
  fixture f;
  REQUIRE(f.f.tree.focus().tab_order().size() == 1);
  f.save->request_focus();
  f.f.tap(key::tab);
  REQUIRE(f.f.tree.focus().tab_order().size() == 1);
}

TEST_CASE("items activate by pointer, Enter and Space", "[menu]") {
  fixture f;
  f.f.click(f.save->bounds().center());
  REQUIRE(f.activated == std::vector<std::string>{"file.save"});
  f.f.tap(key::enter);
  f.f.tap(key::space);
  REQUIRE(f.activated.size() == 3);
}

TEST_CASE("a disabled item stays reachable but does not activate", "[menu]") {
  fixture f;
  f.save->set_enabled(false);
  f.open->request_focus();
  f.f.tap(key::down);
  REQUIRE(f.save->has_focus());
  f.f.tap(key::enter);
  REQUIRE(f.activated.empty());
}

TEST_CASE("Escape asks to dismiss the menu", "[menu]") {
  fixture f;
  f.open->request_focus();
  f.f.tap(key::escape);
  REQUIRE(f.dismissed == 1);
}

TEST_CASE("a menu exposes menu, item and check semantics", "[menu][accessibility]") {
  fixture f;
  auto const snapshot = f.f.semantics();
  auto const *menu_node = snapshot.find(f.popup->id());
  REQUIRE(menu_node != nullptr);
  REQUIRE(menu_node->semantics.kind == a11y::role::menu);
  REQUIRE(menu_node->semantics.name == "File");

  auto const *open_node = snapshot.find(f.open->id());
  REQUIRE(open_node->semantics.kind == a11y::role::menu_item);
  REQUIRE(open_node->semantics.keyboard_shortcut == "Ctrl+O");
  REQUIRE(open_node->semantics.position_in_set == 1);
  REQUIRE(open_node->semantics.set_size == 3);

  auto const *wrap_node = snapshot.find(f.wrap->id());
  REQUIRE(wrap_node->semantics.kind == a11y::role::menu_item_check);
  REQUIRE(wrap_node->semantics.checked == a11y::check_state::checked);
  REQUIRE(wrap_node->semantics.position_in_set == 3);

  REQUIRE(a11y::audit(snapshot).empty());
  REQUIRE(a11y::perform_action(f.f.tree, f.save->id(), {a11y::action::press}));
  REQUIRE(f.activated == std::vector<std::string>{"file.save"});
}

TEST_CASE("a checked item paints a check mark", "[menu][paint]") {
  fixture f;
  REQUIRE(count_commands<line_command>(f.f.commands()) == 2);
  f.wrap->set_checked(false);
  REQUIRE(count_commands<line_command>(f.f.commands()) == 0);
}
