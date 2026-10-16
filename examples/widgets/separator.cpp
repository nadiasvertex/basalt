// A separator between two groups of controls.

#include <print>

#include "example_host.h"

auto main(int argc, char **argv) -> int {
  auto host = example::example_host{{280, 104}};

  host.content().set_cross_alignment(basalt::alignment::stretch);
  host.content().emplace_child<basalt::label>("Account");
  auto &rule = host.content().emplace_child<basalt::separator>();
  host.content().emplace_child<basalt::label>("Notifications");

  host.frame();
  std::println("The separator is {} wide.", rule.bounds().extent.width);

  return host.finish(argc, argv);
}
