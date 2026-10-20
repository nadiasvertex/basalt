#include <catch2/catch_test_macros.hpp>

#include "basalt/widgets/box.h"
#include "basalt/widgets/combo_box.h"
#include "basalt/widgets/overlay_host.h"
#include "basalt/widgets/text_field.h"

#include <memory>
#include <string>
#include <vector>

#include "input_probe.h"
#include "text/font_support.h"
#include "ui_fixture.h"

using namespace basalt;
using namespace basalt::testing;
namespace a11y = basalt::accessibility;

namespace {

struct fixture {
  fixture() {
    host = &f.tree.emplace_root<overlay_host>();
    auto &content = host->set_content(std::make_unique<box>(axis::vertical));
    content.set_cross_alignment(alignment::start);
    field = &content.emplace_child<combo_box>("Group");
    field->set_placeholder("Add");
    field->add_suggestion("In this article", "Body");
    field->add_suggestion("In this article", "Sidebar");
    field->add_suggestion("Standard names", "Headline");
    field->add_suggestion("Standard names", "Body");
    field->add_suggestion("Standard names", "Byline");
    field->set_on_change([this](std::string const &text) { changes.push_back(text); });
    field->set_on_submit([this](std::string const &text) { submitted.push_back(text); });
    f.layout();
    field->input().request_focus();
  }

  ui f;
  overlay_host *host = nullptr;
  combo_box *field = nullptr;
  std::vector<std::string> changes;
  std::vector<std::string> submitted;
};

} // namespace

TEST_CASE("typing filters the suggestions and opens the list", "[combo_box]") {
  fixture f;
  REQUIRE_FALSE(f.field->is_open());
  f.f.type("b");
  REQUIRE(f.field->is_open());
  REQUIRE(f.field->matching_suggestions() == std::vector<std::string>{"Body", "Sidebar", "Byline"});
  REQUIRE(f.changes == std::vector<std::string>{"b"});

  f.f.type("y");
  REQUIRE(f.field->matching_suggestions() == std::vector<std::string>{"Byline"});
}

TEST_CASE("a suggestion in two sections is listed once, in the first", "[combo_box]") {
  fixture f;
  auto const names = f.field->matching_suggestions();
  REQUIRE(names == std::vector<std::string>{"Body", "Sidebar", "Headline", "Byline"});
}

TEST_CASE("Enter submits text that matches no suggestion", "[combo_box]") {
  fixture f;
  f.f.type("Custom");
  REQUIRE_FALSE(f.field->is_open());
  REQUIRE(f.f.tap(key::enter));
  REQUIRE(f.submitted == std::vector<std::string>{"Custom"});
  REQUIRE(f.field->text() == "Custom");
}

TEST_CASE("Down opens the list and Enter takes the highlighted suggestion", "[combo_box]") {
  fixture f;
  REQUIRE(f.f.tap(key::down));
  REQUIRE(f.field->is_open());
  REQUIRE_FALSE(f.field->highlighted_suggestion());

  f.f.tap(key::down);
  f.f.tap(key::down);
  REQUIRE(f.field->highlighted_suggestion() == "Sidebar");
  f.f.tap(key::up);
  REQUIRE(f.field->highlighted_suggestion() == "Body");

  f.f.tap(key::down);
  f.f.tap(key::enter);
  REQUIRE(f.submitted == std::vector<std::string>{"Sidebar"});
  REQUIRE(f.field->text() == "Sidebar");
  REQUIRE_FALSE(f.field->is_open());
  REQUIRE(f.changes.empty());
}

TEST_CASE("Escape closes the list and is left alone when it is closed", "[combo_box]") {
  fixture f;
  f.f.tap(key::down);
  REQUIRE(f.f.tap(key::escape));
  REQUIRE_FALSE(f.field->is_open());
  REQUIRE_FALSE(f.f.tap(key::escape));
}

TEST_CASE("pressing the chevron opens the list and focuses the field", "[combo_box]") {
  fixture f;
  f.field->input().set_text("");
  auto const bounds = f.field->bounds_in_root();
  point const chevron{bounds.right() - 8, bounds.center().y};
  f.f.click(chevron);
  REQUIRE(f.field->is_open());
  REQUIRE(f.field->input().has_focus());
  f.f.click(chevron);
  REQUIRE_FALSE(f.field->is_open());
}

TEST_CASE("clicking a suggestion chooses it and keeps focus in the field", "[combo_box]") {
  fixture f;
  REQUIRE(f.field->open());
  // The rows are: heading, Body, Sidebar, heading, Headline, Byline.
  float const row = f.field->resolve(f.field->theme().sizes.dimension_height_element_min);
  rect const bounds = f.field->bounds_in_root();
  f.f.click({bounds.left() + 10, bounds.bottom() + 4 + row * 2.5f});
  REQUIRE(f.submitted == std::vector<std::string>{"Sidebar"});
  REQUIRE(f.field->text() == "Sidebar");
  REQUIRE_FALSE(f.field->is_open());
  REQUIRE(f.field->input().has_focus());
}

TEST_CASE("losing focus closes the list", "[combo_box]") {
  fixture f;
  f.f.tap(key::down);
  REQUIRE(f.field->is_open());
  f.field->input().set_text("x");
  f.f.tree.focus().clear_focus();
  REQUIRE_FALSE(f.field->is_open());
}

TEST_CASE("set_text and clear_suggestions do not call handlers", "[combo_box]") {
  fixture f;
  f.field->set_text("Sidebar");
  REQUIRE(f.field->matching_suggestions() == std::vector<std::string>{"Sidebar"});
  f.field->clear_suggestions();
  REQUIRE(f.field->suggestion_count() == 0);
  REQUIRE(f.changes.empty());
  REQUIRE(f.submitted.empty());
}

TEST_CASE("combo boxes expose combo box semantics", "[combo_box][accessibility]") {
  fixture f;
  f.field->set_helper_text("Optional");
  f.field->set_text("Body");
  auto snapshot = f.f.semantics();
  auto const *node = snapshot.find(f.field->id());
  REQUIRE(node->semantics.kind == a11y::role::combo_box);
  REQUIRE(node->semantics.name == "Group");
  REQUIRE(node->semantics.description == "Optional");
  REQUIRE(node->semantics.value == "Body");
  REQUIRE(node->semantics.expanded == false);

  REQUIRE(a11y::perform_action(f.f.tree, f.field->id(), {a11y::action::press}));
  REQUIRE(f.f.semantics().find(f.field->id())->semantics.expanded == true);
}
