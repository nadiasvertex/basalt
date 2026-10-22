// A tab bar with more tabs than fit. It shows the scroll buttons, and a simulated click on the
// trailing button scrolls the row.

#include <print>
#include <string>

#include "example_host.h"

auto main(int argc, char **argv) -> int {
  auto host = example::example_host{{420, 72}};

  auto &sections = host.content().emplace_child<basalt::tab_bar>("Sections");
  for (int i = 1; i <= 8; ++i) {
    sections.add_tab("Section " + std::to_string(i)).set_closable(i == 8);
  }
  sections.select(0);
  sections.set_on_select([](std::size_t index) { std::println("Tab {} is selected.", index); });
  sections.set_on_close([](std::size_t index) { std::println("Tab {} asked to close.", index); });

  host.content().set_cross_alignment(basalt::alignment::stretch);
  host.frame();
  sections.scroll_toward_end();
  host.frame();
  std::println("The row is scrolled {} pixels of {}.", sections.scroll_offset(),
               sections.max_scroll_offset());

  return host.finish(argc, argv);
}
