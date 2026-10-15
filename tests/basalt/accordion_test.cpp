#include <catch2/catch_test_macros.hpp>

#include "basalt/widgets/accordion.h"
#include "basalt/widgets/box.h"

#include <memory>
#include <vector>

#include "input_probe.h"
#include "leaf.h"
#include "ui_fixture.h"

using namespace basalt;
using namespace basalt::testing;
namespace a11y = basalt::accessibility;

namespace {

struct fixture {
  fixture() {
    root = &f.tree.emplace_root<box>(axis::vertical);
    root->set_cross_alignment(alignment::start);
    group = &root->emplace_child<accordion>("Settings");
    a = &group->add_item("General", std::make_unique<leaf>(basalt::size{100, 40}));
    b = &group->add_item("Privacy", std::make_unique<leaf>(basalt::size{100, 60}));
    f.layout();
  }

  ui f;
  box *root = nullptr;
  accordion *group = nullptr;
  accordion_item *a = nullptr;
  accordion_item *b = nullptr;
};

} // namespace

TEST_CASE("items start collapsed and hide their content", "[accordion]") {
  fixture f;
  REQUIRE_FALSE(f.a->expanded());
  REQUIRE_FALSE(f.a->content()->visible());
  float const collapsed = f.a->bounds().height();
  f.a->toggle();
  f.f.layout();
  REQUIRE(f.a->content()->visible());
  REQUIRE(f.a->bounds().height() == collapsed + 40);
}

TEST_CASE("a click on the header toggles the item", "[accordion]") {
  fixture f;
  std::vector<bool> seen;
  f.a->set_on_toggle([&seen](bool open) { seen.push_back(open); });
  f.f.click(f.a->header().bounds_in_root().center());
  REQUIRE(f.a->expanded());
  f.f.click(f.a->header().bounds_in_root().center());
  REQUIRE_FALSE(f.a->expanded());
  REQUIRE(seen == std::vector<bool>{true, false});
}

TEST_CASE("Enter and Space toggle the focused header", "[accordion]") {
  fixture f;
  f.f.tap(key::tab);
  REQUIRE(f.a->header().has_focus());
  f.f.tap(key::enter);
  REQUIRE(f.a->expanded());
  f.f.tap(key::space);
  REQUIRE_FALSE(f.a->expanded());
}

TEST_CASE("expand all and collapse all affect every item", "[accordion]") {
  fixture f;
  f.group->expand_all();
  REQUIRE(f.a->expanded());
  REQUIRE(f.b->expanded());
  f.group->collapse_all();
  REQUIRE_FALSE(f.a->expanded());
  REQUIRE_FALSE(f.b->expanded());
}

TEST_CASE("an exclusive accordion keeps one item open", "[accordion]") {
  fixture f;
  f.group->set_exclusive(true);
  f.a->toggle();
  f.b->toggle();
  REQUIRE_FALSE(f.a->expanded());
  REQUIRE(f.b->expanded());
  f.group->expand_all();
  REQUIRE_FALSE(f.a->expanded());
}

TEST_CASE("headers report expanded state and press", "[accordion][accessibility]") {
  fixture f;
  auto snapshot = f.f.semantics();
  auto const *node = snapshot.find(f.a->header().id());
  REQUIRE(node->semantics.kind == a11y::role::button);
  REQUIRE(node->semantics.name == "General");
  REQUIRE(node->semantics.expanded == false);
  REQUIRE(a11y::audit(snapshot).empty());

  REQUIRE(a11y::perform_action(f.f.tree, f.a->header().id(), {a11y::action::press}));
  REQUIRE(f.f.semantics().find(f.a->header().id())->semantics.expanded == true);
}

TEST_CASE("actions are placed at the end of the header", "[accordion]") {
  fixture f;
  auto &action = f.a->set_actions(std::make_unique<leaf>(basalt::size{20, 20}));
  f.f.layout();
  REQUIRE(action.bounds().right() < f.a->header().bounds().width());
  REQUIRE(action.bounds().left() > f.a->header().bounds().width() / 2);
}

TEST_CASE("appearance and chevron placement reach every item", "[accordion]") {
  fixture f;
  f.group->set_appearance(accordion_appearance::filled);
  f.group->set_chevron(chevron_placement::end);
  auto &late = f.group->add_item("Late");
  REQUIRE(late.header().visible());
  REQUIRE(f.group->appearance() == accordion_appearance::filled);
  REQUIRE(f.group->chevron() == chevron_placement::end);
}
