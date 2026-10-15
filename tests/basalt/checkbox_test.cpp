#include <catch2/catch_test_macros.hpp>

#include "basalt/widgets/box.h"
#include "basalt/widgets/checkbox.h"

#include <vector>

#include "ui_fixture.h"

using namespace basalt;
using namespace basalt::testing;
namespace a11y = basalt::accessibility;

namespace {

struct single_fixture {
  single_fixture() {
    root = &f.tree.emplace_root<box>(axis::horizontal);
    root->set_cross_alignment(alignment::start);
    root->set_main_alignment(alignment::start);
    target = &root->emplace_child<checkbox>("Remember me");
    f.layout();
  }

  ui f;
  box *root = nullptr;
  checkbox *target = nullptr;
};

struct group_fixture {
  group_fixture() {
    group = &f.tree.emplace_root<checkbox_group>("Toppings");
    group->enable_select_all("All toppings");
    group->add_option("Cheese");
    group->add_option("Onion");
    group->add_option("Olives");
    f.layout();
  }

  [[nodiscard]] auto select_all() const -> checkbox & {
    return static_cast<checkbox &>(group->child(0));
  }

  ui f;
  checkbox_group *group = nullptr;
};

} // namespace

TEST_CASE("a click checks and unchecks a checkbox", "[checkbox]") {
  single_fixture f;
  REQUIRE(f.target->state() == a11y::check_state::unchecked);
  f.f.click(f.target->bounds().center());
  REQUIRE(f.target->checked());
  f.f.click(f.target->bounds().center());
  REQUIRE(f.target->state() == a11y::check_state::unchecked);
}

TEST_CASE("activating a mixed checkbox checks it", "[checkbox]") {
  single_fixture f;
  f.target->set_state(a11y::check_state::mixed);
  REQUIRE(f.target->activate());
  REQUIRE(f.target->state() == a11y::check_state::checked);
}

TEST_CASE("Space toggles a focused checkbox but Enter does not", "[checkbox]") {
  single_fixture f;
  f.f.tap(key::tab);
  f.f.tap(key::enter);
  REQUIRE_FALSE(f.target->checked());
  f.f.tap(key::space);
  REQUIRE(f.target->checked());
}

TEST_CASE("a checkbox reports its value and error state", "[checkbox][accessibility]") {
  single_fixture f;
  f.target->set_state(a11y::check_state::mixed);
  f.target->set_error(true);
  f.target->set_helper_text("Choose at least one");
  auto const snapshot = f.f.semantics();
  auto const *node = snapshot.find(f.target->id());
  REQUIRE(node != nullptr);
  REQUIRE(node->semantics.kind == a11y::role::check_box);
  REQUIRE(node->semantics.name == "Remember me");
  REQUIRE(node->semantics.checked == a11y::check_state::mixed);
  REQUIRE(node->semantics.invalid);
  REQUIRE(node->semantics.description == "Choose at least one");
}

TEST_CASE("an error colors the unchecked border", "[checkbox][paint]") {
  single_fixture f;
  auto const border = [&] {
    for (auto const &c : f.f.commands()) {
      if (auto const *r = std::get_if<stroke_rounded_rect_command>(&c)) {
        return r->color;
      }
    }
    return rgba{};
  };
  auto const &colors = f.f.tree.theme().colors;
  REQUIRE(border() == colors.border_default);
  f.target->set_error(true);
  REQUIRE(border() == colors.border_danger);
}

TEST_CASE("checking the select-all box checks every option", "[checkbox_group]") {
  group_fixture f;
  REQUIRE(f.group->option_count() == 3);
  f.f.click(f.select_all().bounds().center());
  REQUIRE(f.group->checked_count() == 3);
  f.f.click(f.select_all().bounds().center());
  REQUIRE(f.group->checked_count() == 0);
}

TEST_CASE("select-all is mixed while only some options are checked", "[checkbox_group]") {
  group_fixture f;
  std::vector<std::size_t> changed;
  f.group->set_on_change([&](std::size_t index) { changed.push_back(index); });
  f.f.click(f.group->option(1).bounds().center());
  REQUIRE(f.select_all().state() == a11y::check_state::mixed);
  REQUIRE(changed == std::vector<std::size_t>{1});

  f.group->option(0).activate();
  f.group->option(2).activate();
  REQUIRE(f.select_all().state() == a11y::check_state::checked);
  f.group->option(0).activate();
  f.group->option(1).activate();
  f.group->option(2).activate();
  REQUIRE(f.select_all().state() == a11y::check_state::unchecked);
}
