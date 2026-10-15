#include <catch2/catch_test_macros.hpp>

#include "basalt/widgets/box.h"
#include "basalt/widgets/overlay_host.h"
#include "basalt/widgets/popover.h"

#include <chrono>
#include <memory>

#include "input_probe.h"
#include "leaf.h"
#include "ui_fixture.h"

using namespace basalt;
using namespace basalt::testing;
using namespace std::chrono_literals;

namespace {

struct fixture {
  explicit fixture(popover_trigger kind = popover_trigger::click) {
    host = &f.tree.emplace_root<overlay_host>();
    content = &host->set_content(std::make_unique<box>(axis::vertical));
    content->set_main_alignment(alignment::start);
    content->set_cross_alignment(alignment::start);
    first = &content->emplace_child<popover>(kind);
    first->emplace_trigger<leaf>(basalt::size{80, 30}).set_focus_policy(focus_policy::strong);
    first->set_text("Hello");
    first->set_on_visibility_changed([this](bool shown) { visibility.push_back(shown); });
    f.layout();
  }

  ui f;
  overlay_host *host = nullptr;
  box *content = nullptr;
  popover *first = nullptr;
  std::vector<bool> visibility;
};

} // namespace

TEST_CASE("a click opens the panel and Escape closes it", "[popover]") {
  fixture f;
  REQUIRE_FALSE(f.first->is_open());
  f.f.click(f.first->bounds().center());
  REQUIRE(f.first->is_open());
  REQUIRE(f.host->overlay_count() == 1);
  f.f.tap(key::escape);
  REQUIRE_FALSE(f.first->is_open());
  REQUIRE(f.visibility == std::vector<bool>{true, false});
}

TEST_CASE("Enter on the focused trigger opens the panel", "[popover]") {
  fixture f;
  f.f.tap(key::tab);
  f.f.tap(key::enter);
  REQUIRE(f.first->is_open());
}

TEST_CASE("the panel sits below the trigger and keeps to the width tokens", "[popover]") {
  fixture f;
  f.first->open();
  f.f.layout();
  auto const &sizes = f.f.tree.theme().sizes;
  widget const &layer = f.host->child(f.host->child_count() - 1);
  widget const &panel = layer.child(0);
  REQUIRE(panel.bounds().top() >= f.first->bounds().bottom());
  // The tip is above the body, so the content starts lower in the panel.
  REQUIRE(panel.child(0).bounds().top() > f.first->resolve(sizes.inset_vertical_panel));
  REQUIRE(panel.bounds().width() >= f.first->resolve(sizes.dimension_width_popover_min));
  REQUIRE(panel.bounds().width() <= f.first->resolve(sizes.dimension_width_popover_max));
}

TEST_CASE("the panel opens above a trigger at the bottom of the window", "[popover]") {
  fixture f;
  f.content->set_main_alignment(alignment::end);
  f.f.layout();
  f.first->open();
  f.f.layout();
  widget const &panel = f.host->child(f.host->child_count() - 1).child(0);
  REQUIRE(panel.bounds().bottom() <= f.first->bounds().top() + 0.5f);
  REQUIRE(panel.child(0).bounds().top() ==
          f.first->resolve(f.f.tree.theme().sizes.inset_vertical_panel));
}

TEST_CASE("opening one popover closes another", "[popover]") {
  fixture f;
  auto &second = f.content->emplace_child<popover>(popover_trigger::click);
  second.emplace_trigger<leaf>(basalt::size{80, 30});
  second.set_text("World");
  f.f.layout();
  f.first->open();
  REQUIRE(f.first->is_open());
  REQUIRE(second.open());
  REQUIRE(second.is_open());
  REQUIRE_FALSE(f.first->is_open());
  REQUIRE(f.host->overlay_count() == 1);
}

TEST_CASE("a hover popover opens after the delay and passes the pointer through", "[popover]") {
  fixture f{popover_trigger::hover};
  f.first->set_hover_delay(200ms);
  f.f.send(move(f.first->bounds().center()));
  f.f.advance(100ms);
  REQUIRE_FALSE(f.first->is_open());
  f.f.advance(150ms);
  REQUIRE(f.first->is_open());
  f.f.send(move({390, 290}));
  REQUIRE_FALSE(f.first->is_open());
}

TEST_CASE("a popover without content or a host does not open", "[popover]") {
  fixture f;
  f.first->set_content({});
  REQUIRE_FALSE(f.first->open());
  ui bare;
  auto &lone = bare.tree.emplace_root<popover>();
  lone.set_text("x");
  REQUIRE_FALSE(lone.open());
}

TEST_CASE("content is built every time the panel opens", "[popover]") {
  fixture f;
  int built = 0;
  f.first->set_content([&built] {
    ++built;
    return std::make_unique<leaf>(basalt::size{40, 20});
  });
  f.first->open();
  f.first->close();
  f.first->open();
  REQUIRE(built == 2);
}

TEST_CASE("removing the trigger's popover closes the panel", "[popover]") {
  fixture f;
  f.first->open();
  REQUIRE(f.host->overlay_count() == 1);
  f.f.tree.destroy_later(*f.first);
  f.f.layout();
  REQUIRE(f.host->overlay_count() == 0);
}
