// A tab bar. A simulated click selects the second tab.

#include <print>

#include "example_host.h"

auto main(int argc, char **argv) -> int {
  auto host = example::example_host{{420, 72}};

  auto &sections = host.content().emplace_child<basalt::tab_bar>("Sections");
  sections.add_tab("Overview");
  sections.add_tab("Activity");
  sections.add_tab("Settings").set_closable(true);
  sections.select(0);
  sections.set_on_select([](std::size_t index) { std::println("Tab {} is selected.", index); });
  sections.set_on_close([](std::size_t index) { std::println("Tab {} asked to close.", index); });

  host.content().set_cross_alignment(basalt::alignment::stretch);
  host.frame();
  host.click(sections.tab_at(1));

  return host.finish(argc, argv);
}
