// A checkbox group with a select-all box. A simulated click checks the second option.

#include <print>

#include "example_host.h"

auto main(int argc, char **argv) -> int {
  auto host = example::example_host{{200, 168}};

  auto &toppings = host.content().emplace_child<basalt::checkbox_group>("Toppings");
  toppings.enable_select_all("All toppings");
  toppings.add_option("Cheese", true);
  toppings.add_option("Mushrooms");
  toppings.add_option("Olives");
  toppings.set_on_change([&toppings](std::size_t index) {
    std::println("{} changed. {} option(s) are checked.", toppings.option(index).label(),
                 toppings.checked_count());
  });

  host.frame();
  host.click(toppings.option(1));

  return host.finish(argc, argv);
}
