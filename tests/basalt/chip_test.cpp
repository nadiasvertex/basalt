#include <catch2/catch_test_macros.hpp>

#include "basalt/widgets/box.h"
#include "basalt/widgets/chip.h"
#include "basalt/widgets/overlay_host.h"

#include <chrono>
#include <memory>
#include <vector>

#include "text/font_support.h"
#include "ui_fixture.h"

using namespace basalt;
using namespace basalt::testing;
using namespace std::chrono_literals;
namespace a11y = basalt::accessibility;

namespace {

struct fixture {
  explicit fixture(chip_selection selection = chip_selection::multiple) {
    host = &f.tree.emplace_root<overlay_host>();
    auto &content = host->set_content(std::make_unique<box>(axis::vertical));
    content.set_cross_alignment(alignment::start);
    content.set_main_alignment(alignment::start);
    group = &content.emplace_child<chip_group>("Filters", selection);
    group->set_on_change([this](std::size_t index) { changes.push_back(index); });
    group->add_chip("Red");
    group->add_chip("Green");
    group->add_chip("Blue");
    f.layout();
  }

  ui f;
  overlay_host *host = nullptr;
  chip_group *group = nullptr;
  std::vector<std::size_t> changes;
};

} // namespace

TEST_CASE("chips in a group flow left to right", "[chip]") {
  fixture f;
  REQUIRE(f.group->chip_count() == 3);
  REQUIRE(f.group->chip_at(1).bounds().left() > f.group->chip_at(0).bounds().left());
  REQUIRE(f.group->chip_at(1).bounds().top() == f.group->chip_at(0).bounds().top());
}

TEST_CASE("chips wrap when the group is narrow", "[chip]") {
  auto &fonts = basalt::test::bundled_fonts();
  text::text_shaper shaper{fonts};
  fixture f;
  f.f.tree.set_text_shaper(&shaper);
  f.f.layout();
  float const one = f.group->chip_at(0).bounds().width();
  f.f.tree.set_viewport({one * 2, 300});
  f.f.layout();
  REQUIRE(f.group->chip_at(2).bounds().top() > f.group->chip_at(0).bounds().top());
  f.f.tree.set_text_shaper(nullptr);
}

TEST_CASE("multiple selection toggles chips independently", "[chip]") {
  fixture f;
  f.f.click(f.group->chip_at(0).bounds_in_root().center());
  f.f.click(f.group->chip_at(2).bounds_in_root().center());
  REQUIRE(f.group->selected_indices() == std::vector<std::size_t>{0, 2});
  f.f.click(f.group->chip_at(0).bounds_in_root().center());
  REQUIRE(f.group->selected_indices() == std::vector<std::size_t>{2});
  REQUIRE(f.changes == std::vector<std::size_t>{0, 2, 0});
}

TEST_CASE("single selection keeps one chip selected", "[chip]") {
  fixture f{chip_selection::single};
  f.f.click(f.group->chip_at(0).bounds_in_root().center());
  f.f.click(f.group->chip_at(1).bounds_in_root().center());
  REQUIRE(f.group->selected_indices() == std::vector<std::size_t>{1});
}

TEST_CASE("Space toggles the focused chip", "[chip]") {
  fixture f;
  f.group->chip_at(1).request_focus();
  f.f.tap(key::space);
  REQUIRE(f.group->chip_at(1).selected());
}

TEST_CASE("a chip without selection mode does not toggle", "[chip]") {
  fixture f{chip_selection::none};
  f.f.click(f.group->chip_at(0).bounds_in_root().center());
  REQUIRE_FALSE(f.group->chip_at(0).selected());
  REQUIRE(f.changes.empty());
}

TEST_CASE("a loading chip ignores activation and reports busy", "[chip][accessibility]") {
  fixture f;
  f.group->chip_at(0).set_loading(true);
  REQUIRE_FALSE(f.group->chip_at(0).toggle());
  auto const snapshot = f.f.semantics();
  REQUIRE(snapshot.find(f.group->chip_at(0).id())->semantics.busy);
}

TEST_CASE("removing a chip takes it out of the group", "[chip]") {
  fixture f{chip_selection::none};
  int removed = 0;
  f.group->chip_at(1).set_removable(true);
  f.group->chip_at(1).set_on_remove([&] { ++removed; });
  f.group->chip_at(1).request_focus();
  f.f.tap(key::delete_forward);
  f.f.tree.collect_garbage();
  REQUIRE(removed == 1);
  REQUIRE(f.group->chip_count() == 2);
  REQUIRE(f.group->chip_at(1).label() == "Blue");
}

TEST_CASE("pressing the remove mark removes the chip", "[chip]") {
  fixture f{chip_selection::none};
  chip &target = f.group->chip_at(0);
  target.set_removable(true);
  f.f.layout();
  rect const area = target.bounds_in_root();
  f.f.click({area.right() - 4, area.center().y});
  f.f.tree.collect_garbage();
  REQUIRE(f.group->chip_count() == 2);
}

TEST_CASE("Remove All removes every removable chip", "[chip]") {
  fixture f{chip_selection::none};
  for (std::size_t i = 0; i < 3; ++i) {
    f.group->chip_at(i).set_removable(true);
  }
  f.group->set_remove_all_label("Remove all");
  f.f.layout();
  REQUIRE(f.group->child_count() == 4);
  f.group->remove_all();
  f.f.tree.collect_garbage();
  f.f.layout();
  REQUIRE(f.group->chip_count() == 0);
  REQUIRE(f.group->child_count() == 0);
}

TEST_CASE("a truncated chip shows its label in a tooltip", "[chip][tooltip]") {
  auto &fonts = basalt::test::bundled_fonts();
  text::text_shaper shaper{fonts};
  fixture f;
  f.f.tree.set_text_shaper(&shaper);
  chip &target = f.group->chip_at(0);
  target.set_label("A very long chip label that cannot fit");
  target.set_max_width(fixed(80));
  f.f.layout();
  REQUIRE(target.bounds().width() <= 80);

  f.f.send(move(target.bounds_in_root().center()));
  f.f.advance(800ms);
  REQUIRE(f.host->overlay_count() == 1);
  f.f.tree.set_text_shaper(nullptr);
}

TEST_CASE("chips report selection and the group its name", "[chip][accessibility]") {
  fixture f;
  f.group->chip_at(2).set_selected(true);
  auto const snapshot = f.f.semantics();
  REQUIRE(snapshot.find(f.group->id())->semantics.name == "Filters");
  auto const *node = snapshot.find(f.group->chip_at(2).id());
  REQUIRE(node->semantics.kind == a11y::role::check_box);
  REQUIRE(node->semantics.checked == a11y::check_state::checked);
  REQUIRE(a11y::audit(snapshot).empty());
}
