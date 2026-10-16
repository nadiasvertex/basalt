// The four button variants. A simulated click presses the primary button.

#include <print>

#include "example_host.h"

auto main(int argc, char **argv) -> int {
  auto host = example::example_host{{400, 72}};

  auto &row = host.content().emplace_child<basalt::box>(basalt::axis::horizontal);
  row.set_gap(basalt::fixed(8));

  auto presses = 0;
  auto &save = row.emplace_child<basalt::button>("Save");
  save.set_variant(basalt::button_variant::primary);
  save.set_shortcut("Ctrl+S");
  save.set_on_press([&presses] { ++presses; });
  row.emplace_child<basalt::button>("Cancel").set_variant(basalt::button_variant::secondary);
  row.emplace_child<basalt::button>("Skip").set_variant(basalt::button_variant::tertiary);
  row.emplace_child<basalt::button>("Delete").set_variant(basalt::button_variant::danger);

  host.frame();
  host.click(save);
  std::println("The primary button was pressed {} time(s).", presses);

  return host.finish(argc, argv);
}
