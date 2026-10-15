#include <catch2/catch_test_macros.hpp>

#include "basalt/widgets/button.h"
#include "basalt/widgets/overlay_host.h"

#include <memory>

#include "leaf.h"
#include "ui_fixture.h"

using namespace basalt;
using namespace basalt::testing;

namespace {

struct fixture {
  fixture() {
    host = &f.tree.emplace_root<overlay_host>();
    target = &host->set_content(std::make_unique<button>("Save", [this] { ++presses; }));
    f.layout();
  }

  ui f;
  overlay_host *host = nullptr;
  button *target = nullptr;
  int presses = 0;
};

} // namespace

TEST_CASE("a hover popup lets the pointer reach the content", "[overlay_host]") {
  fixture f;
  f.target->request_focus();
  int closed = 0;
  auto const id = f.host->show_hover_popup(std::make_unique<leaf>(basalt::size{40, 20}),
                                           {{300, 200}, {10, 10}}, [&] { ++closed; });
  f.f.layout();
  REQUIRE(f.host->is_open(id));

  point const inside = f.target->bounds().center();
  f.f.click(inside);
  REQUIRE(f.presses == 1);
  REQUIRE(f.host->is_open(id));
  REQUIRE(f.target->has_focus());

  REQUIRE(f.host->dismiss(id));
  REQUIRE(closed == 1);
  REQUIRE_FALSE(f.host->is_open(id));
}

TEST_CASE("a hover popup keeps focus and ignores Escape", "[overlay_host]") {
  fixture f;
  f.target->request_focus();
  auto const id =
      f.host->show_hover_popup(std::make_unique<leaf>(basalt::size{40, 20}), {{0, 0}, {10, 10}});
  f.f.layout();
  f.f.tap(key::escape);
  REQUIRE(f.host->is_open(id));
  REQUIRE(f.target->has_focus());
}

TEST_CASE("a modal popup still blocks the pointer", "[overlay_host]") {
  fixture f;
  auto const id =
      f.host->show_popup(std::make_unique<leaf>(basalt::size{40, 20}), {{300, 200}, {10, 10}});
  f.f.layout();
  f.f.click(f.target->bounds().center());
  REQUIRE(f.presses == 0);
  REQUIRE_FALSE(f.host->is_open(id));
}

TEST_CASE("a hover popup is placed below its anchor", "[overlay_host]") {
  fixture f;
  rect const anchor{{20, 30}, {50, 10}};
  f.host->show_hover_popup(std::make_unique<leaf>(basalt::size{40, 20}), anchor);
  f.f.layout();
  widget &shown = f.host->child(f.host->child_count() - 1).child(0);
  REQUIRE(shown.bounds().origin.x == 20);
  REQUIRE(shown.bounds().origin.y == anchor.bottom());
}
