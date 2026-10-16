// A chip group with multiple selection. A simulated click selects the first chip.

#include <print>

#include "example_host.h"

auto main(int argc, char **argv) -> int {
  auto host = example::example_host{{360, 64}};

  auto &filters =
      host.content().emplace_child<basalt::chip_group>("Filters", basalt::chip_selection::multiple);
  filters.add_chip("Open").set_selectable(true);
  filters.add_chip("Closed").set_selectable(true);
  filters.add_chip("Archived").set_removable(true);
  filters.set_on_change([&filters](std::size_t index) {
    std::println("{} is {}.", filters.chip_at(index).label(),
                 filters.chip_at(index).selected() ? "selected" : "not selected");
  });

  host.content().set_cross_alignment(basalt::alignment::stretch);
  host.frame();
  host.click(filters.chip_at(0));

  return host.finish(argc, argv);
}
