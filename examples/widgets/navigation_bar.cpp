// A navigation bar with sections, a utility link and an avatar. A simulated click chooses Home.

#include <print>

#include "example_host.h"

auto main(int argc, char **argv) -> int {
  auto host = example::example_host{{640, 80}};

  auto &bar = host.content().emplace_child<basalt::navigation_bar>("Main navigation");
  bar.set_title("Basalt");
  bar.add_section("home", "Home");
  bar.add_section("docs", "Docs").add_entry("api", "API reference");
  bar.add_utility_link("help", "Help");
  bar.set_avatar(std::make_unique<basalt::avatar>("Ada Lovelace", basalt::control_size::small));
  bar.set_on_navigate([](std::string const &id) { std::println("Navigate to {}.", id); });

  host.content().set_cross_alignment(basalt::alignment::stretch);
  host.frame();
  host.click(*bar.find_item("home"));
  std::println("The selected destination is {}.", bar.selected_id().value_or("none"));

  return host.finish(argc, argv);
}
