#include <catch2/catch_test_macros.hpp>

#include "basalt/widgets/box.h"
#include "basalt/widgets/overlay_host.h"
#include "basalt/widgets/tooltip.h"

#include <chrono>
#include <memory>

#include "leaf.h"
#include "ui_fixture.h"

using namespace basalt;
using namespace basalt::testing;
using namespace std::chrono_literals;
namespace a11y = basalt::accessibility;

namespace {

// A widget that forwards to a `tooltip_anchor` the way real owners do.
class tip_target : public widget {
public:
  tip_target() {
    set_focus_policy(focus_policy::strong);
    set_text("Full name");
  }
  auto set_text(std::string text) -> void { anchor.set_text(*this, std::move(text)); }
  tooltip_anchor anchor{200ms};

protected:
  [[nodiscard]] auto on_measure(constraints const &) const -> basalt::size override {
    return {60, 30};
  }
  auto on_interaction_changed(interaction_state const &previous) -> void override {
    widget::on_interaction_changed(previous);
    if (anchor.update(*this)) {
      request_animation_frame();
    }
  }
  auto on_animation_frame(std::chrono::nanoseconds elapsed) -> bool override {
    return anchor.advance(*this, elapsed);
  }
  auto on_detached() -> void override { anchor.hide(*this); }
};

struct fixture {
  fixture() {
    host = &f.tree.emplace_root<overlay_host>();
    auto &content = host->set_content(std::make_unique<box>(axis::horizontal));
    content.set_main_alignment(alignment::start);
    content.set_cross_alignment(alignment::start);
    target = &content.emplace_child<tip_target>();
    f.layout();
    inside = target->bounds().center();
  }

  [[nodiscard]] auto tip_shown() const -> bool { return target->anchor.visible(); }

  ui f;
  overlay_host *host = nullptr;
  tip_target *target = nullptr;
  point inside;
};

} // namespace

TEST_CASE("a tooltip appears after the pointer rests on the widget", "[tooltip]") {
  fixture f;
  f.f.send(move(f.inside));
  REQUIRE_FALSE(f.tip_shown());
  f.f.advance(100ms);
  REQUIRE_FALSE(f.tip_shown());
  f.f.advance(150ms);
  REQUIRE(f.tip_shown());
  REQUIRE(f.host->overlay_count() == 1);
}

TEST_CASE("leaving the widget hides the tooltip", "[tooltip]") {
  fixture f;
  f.f.send(move(f.inside));
  f.f.advance(300ms);
  REQUIRE(f.tip_shown());
  f.f.send(move({350, 250}));
  REQUIRE_FALSE(f.tip_shown());
  REQUIRE(f.host->overlay_count() == 0);
}

TEST_CASE("leaving before the delay ends shows nothing", "[tooltip]") {
  fixture f;
  f.f.send(move(f.inside));
  f.f.advance(100ms);
  f.f.send(move({350, 250}));
  f.f.advance(300ms);
  REQUIRE_FALSE(f.tip_shown());
}

TEST_CASE("keyboard focus shows the tooltip", "[tooltip]") {
  fixture f;
  f.f.tap(key::tab);
  REQUIRE(f.target->has_focus());
  f.f.advance(300ms);
  REQUIRE(f.tip_shown());
}

TEST_CASE("an empty text never shows a tooltip", "[tooltip]") {
  fixture f;
  f.target->set_text({});
  f.f.send(move(f.inside));
  f.f.advance(300ms);
  REQUIRE_FALSE(f.tip_shown());
}

TEST_CASE("a tooltip reports its text with the tooltip role", "[tooltip][accessibility]") {
  ui f;
  auto &bubble = f.tree.emplace_root<tooltip>("Full name");
  f.layout();
  REQUIRE(bubble.accessibility_info().kind == a11y::role::tooltip);
  REQUIRE(bubble.accessibility_info().name == "Full name");
}

TEST_CASE("a tooltip is padded by the theme insets", "[tooltip]") {
  ui f;
  auto &bubble = f.tree.emplace_root<tooltip>("Full name");
  f.layout();
  auto const &sizes = f.tree.theme().sizes;
  REQUIRE(bubble.measure(constraints::loose({400, 300})).width >=
          2 * bubble.resolve(sizes.inset_horizontal_tooltip));
}
