#include <catch2/catch_test_macros.hpp>

#include "basalt/widgets/status_bar.h"

#include "ui_fixture.h"

using namespace basalt;
using namespace basalt::testing;
namespace a11y = basalt::accessibility;

namespace {

struct fixture {
  fixture() : f({400, 100}) {
    auto &root = f.tree.set_root(std::make_unique<widget>());
    bar = &root.emplace_child<status_bar>();
    message = &bar->add_section("message", "Ready", 1);
    position = &bar->add_section("position", "Ln 1, Col 1");
    encoding = &bar->add_section("encoding", "UTF-8");
    bar->set_bounds({{0, 76}, {400, 24}});
    f.layout();
  }

  ui f;
  status_bar *bar = nullptr;
  label *message = nullptr;
  label *position = nullptr;
  label *encoding = nullptr;
};

} // namespace

TEST_CASE("the growing section pushes later sections to the end", "[status_bar]") {
  fixture f;
  REQUIRE(f.message->bounds().left() < f.position->bounds().left());
  REQUIRE(f.position->bounds().right() <= f.encoding->bounds().left());
  REQUIRE(f.encoding->bounds().right() <= 400);
  REQUIRE(f.encoding->bounds().right() > 380);
  REQUIRE(f.message->bounds().width() > f.position->bounds().width());
}

TEST_CASE("sections are found and updated by id", "[status_bar]") {
  fixture f;
  REQUIRE(f.bar->section("position") == f.position);
  REQUIRE(f.bar->section("missing") == nullptr);
  REQUIRE(f.bar->set_section_text("position", "Ln 4, Col 2"));
  REQUIRE(f.position->text() == "Ln 4, Col 2");
  REQUIRE_FALSE(f.bar->set_section_text("missing", "x"));
}

TEST_CASE("the status bar is a polite live region of labels", "[status_bar][accessibility]") {
  fixture f;
  auto const snapshot = f.f.semantics();
  auto const *node = snapshot.find(f.bar->id());
  REQUIRE(node != nullptr);
  REQUIRE(node->semantics.kind == a11y::role::status_bar);
  REQUIRE(node->semantics.live == a11y::live_region::polite);
  REQUIRE(node->children.size() == 3);
  REQUIRE(snapshot.find(f.message->id())->semantics.name == "Ready");
}

TEST_CASE("the bar paints a background and a top border", "[status_bar][paint]") {
  fixture f;
  REQUIRE(count_commands<fill_rect_command>(f.f.commands()) == 2);
}
