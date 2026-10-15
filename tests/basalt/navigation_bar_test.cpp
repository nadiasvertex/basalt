#include <catch2/catch_test_macros.hpp>

#include "basalt/widgets/navigation_bar.h"
#include "basalt/widgets/overlay_host.h"

#include <memory>
#include <string>
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
    host = &f.tree.emplace_root<overlay_host>();
    bar = &host->set_content(std::make_unique<navigation_bar>("Main"));
    bar->set_title("Atlas");
    home = &bar->add_section("home", "Home");
    reports = &bar->add_section("reports", "Reports");
    reports->add_entry("reports.monthly", "Monthly");
    reports->add_entry("reports.yearly", "Yearly");
    help = &bar->add_utility_link("help", "Help");
    bar->set_on_navigate([this](std::string const &id) { navigated.push_back(id); });
    f.layout();
  }

  ui f;
  overlay_host *host = nullptr;
  navigation_bar *bar = nullptr;
  navigation_item *home = nullptr;
  navigation_item *reports = nullptr;
  navigation_item *help = nullptr;
  std::vector<std::string> navigated;
};

} // namespace

TEST_CASE("choosing a section selects it and navigates", "[navigation_bar]") {
  fixture f;
  f.f.click(f.home->bounds_in_root().center());
  REQUIRE(f.navigated == std::vector<std::string>{"home"});
  REQUIRE(f.bar->selected_id() == "home");
  REQUIRE(f.home->selected());
  REQUIRE_FALSE(f.reports->selected());
}

TEST_CASE("a utility link navigates without becoming selected", "[navigation_bar]") {
  fixture f;
  f.home->activate();
  f.help->activate();
  REQUIRE(f.navigated == std::vector<std::string>{"home", "help"});
  REQUIRE(f.bar->selected_id() == "home");
  REQUIRE_FALSE(f.help->selected());
}

TEST_CASE("a section with entries opens a submenu", "[navigation_bar]") {
  fixture f;
  REQUIRE(f.reports->activate());
  REQUIRE(f.reports->is_open());
  REQUIRE(f.navigated.empty());
  REQUIRE(f.host->overlay_count() == 1);
  f.f.tap(key::escape);
  REQUIRE_FALSE(f.reports->is_open());
}

TEST_CASE("choosing an entry navigates to it and selects its section", "[navigation_bar]") {
  fixture f;
  f.reports->activate();
  f.f.tap(key::tab);
  f.f.tap(key::enter);
  REQUIRE(f.navigated == std::vector<std::string>{"reports.monthly"});
  REQUIRE(f.bar->selected_id() == "reports");
  REQUIRE_FALSE(f.reports->is_open());
}

TEST_CASE("the toolbar layout hides sections and utility links", "[navigation_bar]") {
  fixture f;
  f.bar->set_layout(navigation_layout::toolbar);
  f.f.layout();
  REQUIRE_FALSE(f.home->visible_in_tree());
  REQUIRE_FALSE(f.help->visible_in_tree());
  f.bar->set_layout(navigation_layout::sections);
  REQUIRE(f.home->visible_in_tree());
}

TEST_CASE("leading actions, center actions and the avatar are placed in order",
          "[navigation_bar]") {
  fixture f;
  auto &lead = f.bar->emplace_leading<leaf>(basalt::size{20, 20});
  auto &mid = f.bar->emplace_center<leaf>(basalt::size{20, 20});
  auto &me = f.bar->set_avatar(std::make_unique<avatar>("Ada Lovelace"));
  me.set_layout(avatar_layout::graphic_only);
  f.f.layout();
  REQUIRE(lead.bounds_in_root().left() < f.home->bounds_in_root().left());
  REQUIRE(f.home->bounds_in_root().right() <= mid.bounds_in_root().left());
  REQUIRE(f.help->bounds_in_root().right() <= me.bounds_in_root().left() + 0.5f);
}

TEST_CASE("the bar exposes navigation semantics", "[navigation_bar][accessibility]") {
  fixture f;
  f.home->activate();
  auto snapshot = f.f.semantics();
  auto const *bar_node = snapshot.find(f.bar->id());
  REQUIRE(bar_node->semantics.kind == a11y::role::navigation);
  REQUIRE(bar_node->semantics.name == "Main");
  auto const *home_node = snapshot.find(f.home->id());
  REQUIRE(home_node->semantics.selected);
  REQUIRE(snapshot.find(f.reports->id())->semantics.expanded == false);
  REQUIRE(a11y::audit(snapshot).empty());
}
