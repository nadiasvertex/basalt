// Avatars with initials, supporting text, and the three layouts.

#include <print>

#include "example_host.h"

auto main(int argc, char **argv) -> int {
  auto host = example::example_host{{400, 72}};

  auto &row = host.content().emplace_child<basalt::box>(basalt::axis::horizontal);
  row.set_gap(basalt::fixed(24));
  row.set_cross_alignment(basalt::alignment::center);

  auto &named = row.emplace_child<basalt::avatar>("Grace Hopper");
  named.set_supporting_text("Rear admiral");
  row.emplace_child<basalt::avatar>("Alan Turing", basalt::control_size::small)
      .set_layout(basalt::avatar_layout::leading);
  row.emplace_child<basalt::avatar>("Katherine Johnson", basalt::control_size::large)
      .set_layout(basalt::avatar_layout::graphic_only);

  host.frame();
  std::println("The initials are {}.", basalt::avatar_initials(named.name()));
  std::println("The graphic is {} layout pixels wide.", named.diameter());

  return host.finish(argc, argv);
}
