#include <catch2/catch_test_macros.hpp>

#include "basalt/widgets/box.h"
#include "basalt/widgets/slider.h"

#include <vector>

#include "text/font_support.h"
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
    target = &root->emplace_child<slider>(0.0f, 100.0f, 1.0f, 50.0f);
    target->set_on_change([this](float v) { changes.push_back(v); });
    target->set_on_commit([this](float v) { commits.push_back(v); });
    f.layout();
  }

  // The point on the track where the slider's value would be `value`.
  [[nodiscard]] auto at(float value) const -> point {
    rect const track = target->track_rect();
    float const x = track.origin.x + track.width() * (value - target->minimum()) /
                                         (target->maximum() - target->minimum());
    return target->to_root({x, target->thumb_center().y});
  }

  ui f;
  box *root = nullptr;
  slider *target = nullptr;
  std::vector<float> changes;
  std::vector<float> commits;
};

} // namespace

TEST_CASE("the thumb sits at the position of the value", "[slider]") {
  fixture f;
  rect const track = f.target->track_rect();
  REQUIRE(f.target->thumb_center().x == track.origin.x + track.width() / 2);
  f.target->set_value(0);
  REQUIRE(f.target->thumb_center().x == track.origin.x);
  f.target->set_value(100);
  REQUIRE(f.target->thumb_center().x == track.origin.x + track.width());
}

TEST_CASE("set_value rounds to a step, clamps and stays quiet", "[slider]") {
  fixture f;
  f.target->set_range(0, 10, 2.5f);
  f.target->set_value(3.4f);
  REQUIRE(f.target->value() == 2.5f);
  f.target->set_value(-5);
  REQUIRE(f.target->value() == 0);
  f.target->set_value(99);
  REQUIRE(f.target->value() == 10);
  REQUIRE(f.changes.empty());
  REQUIRE(f.commits.empty());
}

TEST_CASE("a bad range or step is repaired", "[slider]") {
  slider s{5, 1, 0, 3};
  REQUIRE(s.minimum() == 5);
  REQUIRE(s.maximum() == 5);
  REQUIRE(s.step() > 0);
  REQUIRE(s.value() == 5);
}

TEST_CASE("pressing the track moves the thumb and commits on release", "[slider]") {
  fixture f;
  f.f.send(press(f.at(25)));
  REQUIRE(f.target->dragging());
  REQUIRE(f.target->value() == 25);
  REQUIRE(f.changes == std::vector<float>{25});
  REQUIRE(f.commits.empty());
  f.f.send(release(f.at(25)));
  REQUIRE_FALSE(f.target->dragging());
  REQUIRE(f.commits == std::vector<float>{25});
}

TEST_CASE("dragging reports each new value and commits once at the end", "[slider]") {
  fixture f;
  f.f.send(press(f.at(50)));
  f.f.send(move(f.at(60), pointer_button::primary));
  f.f.send(move(f.at(70), pointer_button::primary));
  f.f.send(move(f.at(70), pointer_button::primary));
  REQUIRE(f.changes == std::vector<float>{60, 70});
  REQUIRE(f.commits.empty());
  f.f.send(release(f.at(70)));
  REQUIRE(f.commits == std::vector<float>{70});
}

TEST_CASE("a drag past the ends stops at the range", "[slider]") {
  fixture f;
  f.f.send(press(f.at(50)));
  f.f.send(move(point{f.at(100).x + 300, f.at(100).y}, pointer_button::primary));
  REQUIRE(f.target->value() == 100);
  f.f.send(move(point{f.at(0).x - 300, f.at(0).y}, pointer_button::primary));
  REQUIRE(f.target->value() == 0);
}

TEST_CASE("Escape during a drag restores the value without a commit", "[slider]") {
  fixture f;
  f.f.send(press(f.at(80)));
  REQUIRE(f.target->value() == 80);
  f.f.tap(key::escape);
  REQUIRE(f.target->value() == 50);
  REQUIRE_FALSE(f.target->dragging());
  REQUIRE(f.changes == std::vector<float>{80, 50});
  REQUIRE(f.commits.empty());
}

TEST_CASE("arrow, page, Home and End keys change a focused slider", "[slider]") {
  fixture f;
  f.f.tap(key::tab);
  REQUIRE(f.target->has_focus());
  f.f.tap(key::right);
  REQUIRE(f.target->value() == 51);
  f.f.tap(key::up);
  REQUIRE(f.target->value() == 52);
  f.f.tap(key::left);
  f.f.tap(key::down);
  REQUIRE(f.target->value() == 50);
  f.f.tap(key::page_up);
  REQUIRE(f.target->value() == 60);
  f.f.tap(key::page_down);
  f.f.tap(key::page_down);
  REQUIRE(f.target->value() == 40);
  f.f.tap(key::home);
  REQUIRE(f.target->value() == 0);
  f.f.tap(key::end);
  REQUIRE(f.target->value() == 100);
  REQUIRE(f.commits.size() == f.changes.size());
}

TEST_CASE("a key at the end of the range commits without a change", "[slider]") {
  fixture f;
  f.target->set_value(100);
  f.f.tap(key::tab);
  f.f.tap(key::right);
  REQUIRE(f.changes.empty());
  REQUIRE(f.commits == std::vector<float>{100});
}

TEST_CASE("a disabled slider ignores input", "[slider]") {
  fixture f;
  f.target->set_enabled(false);
  f.f.click(f.at(10));
  REQUIRE(f.target->value() == 50);
  REQUIRE(f.changes.empty());
}

TEST_CASE("a slider takes keyboard focus through Tab", "[slider]") {
  fixture f;
  REQUIRE(f.target->accepts_focus());
  f.f.tap(key::tab);
  REQUIRE(f.target->has_focus());
}

TEST_CASE("a label and value text make the slider taller and wider", "[slider]") {
  text::text_shaper shaper{basalt::test::bundled_fonts()};
  ui f;
  f.tree.set_text_shaper(&shaper);
  auto &root = f.tree.emplace_root<box>(axis::horizontal);
  auto &target = root.emplace_child<slider>(0.0f, 1.0f, 0.1f, 0.5f);
  f.layout();
  constexpr constraints wide{{}, {500, 200}};
  auto const bare = target.measure(wide);

  target.set_label("Blur threshold");
  target.set_value_formatter([](float v) { return std::to_string(v); });
  auto const labeled = target.measure(wide);
  REQUIRE(labeled.height > bare.height);
  REQUIRE(labeled.width >= bare.width);
  REQUIRE(target.label() == "Blur threshold");
}

TEST_CASE("a slider reports its range and value to assistive technology",
          "[slider][accessibility]") {
  fixture f;
  f.target->set_label("Volume");
  f.target->set_value_formatter([](float v) { return std::to_string(static_cast<int>(v)) + "%"; });
  auto const info = f.target->accessibility_info();
  REQUIRE(info.kind == a11y::role::slider);
  REQUIRE(info.name == "Volume");
  REQUIRE(info.value == "50%");
  REQUIRE(info.numeric_value == 50.0);
  REQUIRE(info.range == a11y::value_range{0, 100, 1});
  REQUIRE(info.actions.contains(a11y::action::increment));
  REQUIRE(info.actions.contains(a11y::action::decrement));
}

TEST_CASE("increment and decrement actions step the value and commit", "[slider][accessibility]") {
  fixture f;
  REQUIRE(f.target->perform_accessibility_action({.kind = a11y::action::increment}));
  REQUIRE(f.target->value() == 51);
  REQUIRE(f.target->perform_accessibility_action({.kind = a11y::action::decrement}));
  REQUIRE(f.target->value() == 50);
  REQUIRE(f.commits == std::vector<float>{51, 50});
}

TEST_CASE("painting a slider records a track, a fill and a thumb", "[slider][paint]") {
  fixture f;
  auto const commands = f.f.commands();
  REQUIRE(count_commands<fill_rounded_rect_command>(commands) >= 3);
}
