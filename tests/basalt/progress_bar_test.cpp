#include <catch2/catch_test_macros.hpp>

#include "basalt/widgets/progress_bar.h"

#include "ui_fixture.h"

using namespace basalt;
using namespace basalt::testing;
namespace a11y = basalt::accessibility;

namespace {

struct fixture {
  fixture() : f({200, 40}) {
    auto &root = f.tree.set_root(std::make_unique<widget>());
    bar = &root.emplace_child<progress_bar>();
    bar->set_bounds({{10, 10}, {120, 6}});
    f.layout();
  }

  ui f;
  progress_bar *bar = nullptr;
};

} // namespace

TEST_CASE("a progress bar starts indeterminate", "[progress_bar]") {
  fixture f;
  REQUIRE_FALSE(f.bar->fraction().has_value());
}

TEST_CASE("the fraction is clamped to the range 0 to 1", "[progress_bar]") {
  fixture f;
  f.bar->set_fraction(1.5f);
  REQUIRE(*f.bar->fraction() == 1.0f);
  f.bar->set_fraction(-0.5f);
  REQUIRE(*f.bar->fraction() == 0.0f);
  f.bar->set_fraction(std::nullopt);
  REQUIRE_FALSE(f.bar->fraction().has_value());
}

TEST_CASE("a progress bar reports its value to assistive technology",
          "[progress_bar][accessibility]") {
  fixture f;
  f.bar->set_fraction(0.25f);
  auto snapshot = f.f.semantics();
  auto const *node = snapshot.find(f.bar->id());
  REQUIRE(node != nullptr);
  REQUIRE(node->semantics.kind == a11y::role::progress_bar);
  REQUIRE(node->semantics.busy);
  REQUIRE(node->semantics.numeric_value.has_value());
  REQUIRE(*node->semantics.numeric_value == 25.0);

  f.bar->set_fraction(1.0f);
  snapshot = f.f.semantics();
  REQUIRE_FALSE(snapshot.find(f.bar->id())->semantics.busy);

  f.bar->set_fraction(std::nullopt);
  snapshot = f.f.semantics();
  REQUIRE(snapshot.find(f.bar->id())->semantics.busy);
  REQUIRE_FALSE(snapshot.find(f.bar->id())->semantics.numeric_value.has_value());
}

TEST_CASE("the bar paints a track and a fill", "[progress_bar][paint]") {
  fixture f;
  auto const track_only = count_commands<fill_rounded_rect_command>(f.f.commands());
  f.bar->set_fraction(0.5f);
  f.f.layout();
  REQUIRE(count_commands<fill_rounded_rect_command>(f.f.commands()) == track_only + 1);
}
