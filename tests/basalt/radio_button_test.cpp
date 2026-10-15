#include <catch2/catch_test_macros.hpp>

#include "basalt/widgets/box.h"
#include "basalt/widgets/radio_button.h"

#include <vector>

#include "ui_fixture.h"

using namespace basalt;
using namespace basalt::testing;
namespace a11y = basalt::accessibility;

namespace {

struct fixture {
  fixture() {
    root = &f.tree.emplace_root<box>(axis::vertical);
    root->set_cross_alignment(alignment::start);
    group = &root->emplace_child<radio_group>("Size");
    group->add_option("Small");
    group->add_option("Medium");
    group->add_option("Large");
    group->set_on_change([this](std::size_t index) { changes.push_back(index); });
    f.layout();
  }

  ui f;
  box *root = nullptr;
  radio_group *group = nullptr;
  std::vector<std::size_t> changes;
};

} // namespace

TEST_CASE("selecting an option deselects the others", "[radio]") {
  fixture f;
  f.f.click(f.group->option(0).bounds_in_root().center());
  REQUIRE(f.group->selected_index() == 0);
  f.f.click(f.group->option(2).bounds_in_root().center());
  REQUIRE(f.group->selected_index() == 2);
  REQUIRE_FALSE(f.group->option(0).selected());
  REQUIRE(f.changes == std::vector<std::size_t>{0, 2});
}

TEST_CASE("pressing the selected option again changes nothing", "[radio]") {
  fixture f;
  f.group->select(1);
  f.group->option(1).activate();
  REQUIRE(f.group->selected_index() == 1);
  REQUIRE(f.changes.empty());
}

TEST_CASE("selecting from code is silent and checks the range", "[radio]") {
  fixture f;
  REQUIRE(f.group->select(1));
  REQUIRE(f.changes.empty());
  REQUIRE_FALSE(f.group->select(3));
  REQUIRE(f.group->selected_index() == 1);
}

TEST_CASE("arrow keys move focus and selection through the group", "[radio]") {
  fixture f;
  f.group->option(0).request_focus();
  f.f.tap(key::down);
  REQUIRE(f.group->option(1).has_focus());
  REQUIRE(f.group->selected_index() == 1);
  f.f.tap(key::right);
  REQUIRE(f.group->selected_index() == 2);
  f.f.tap(key::down);
  REQUIRE(f.group->selected_index() == 0);
  f.f.tap(key::up);
  REQUIRE(f.group->selected_index() == 2);
}

TEST_CASE("the group is one Tab stop", "[radio]") {
  fixture f;
  f.group->select(1);
  f.f.tap(key::tab);
  REQUIRE(f.group->option(0).has_focus());
  f.f.tap(key::tab);
  REQUIRE_FALSE(f.group->option(1).has_focus());
  REQUIRE_FALSE(f.group->option(2).has_focus());
}

TEST_CASE("radio buttons report selection and group name", "[radio][accessibility]") {
  fixture f;
  f.group->select(1);
  auto const snapshot = f.f.semantics();
  auto const *group_node = snapshot.find(f.group->id());
  REQUIRE(group_node != nullptr);
  REQUIRE(group_node->semantics.kind == a11y::role::group);
  REQUIRE(group_node->semantics.name == "Size");
  auto const *node = snapshot.find(f.group->option(1).id());
  REQUIRE(node != nullptr);
  REQUIRE(node->semantics.kind == a11y::role::radio_button);
  REQUIRE(node->semantics.checked == a11y::check_state::checked);
  REQUIRE(a11y::audit(snapshot).empty());
}
