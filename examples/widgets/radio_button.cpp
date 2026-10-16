// A radio group. A simulated click selects the third option.

#include <print>

#include "example_host.h"

auto main(int argc, char **argv) -> int {
  auto host = example::example_host{{160, 144}};

  auto &size = host.content().emplace_child<basalt::radio_group>("Size");
  size.add_option("Small");
  size.add_option("Medium");
  size.add_option("Large");
  size.select(1);
  size.set_on_change(
      [&size](std::size_t index) { std::println("{} is selected.", size.option(index).label()); });

  host.frame();
  host.click(size.option(2));

  return host.finish(argc, argv);
}
