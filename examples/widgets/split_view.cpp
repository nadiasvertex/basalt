// A resizable split view. The program moves the divider by a step.

#include <print>

#include "example_host.h"

auto main(int argc, char **argv) -> int {
  auto host = example::example_host{{420, 152}};

  auto &split = host.content().add(std::make_unique<basalt::split_view>(basalt::axis::horizontal),
                                   basalt::box_item{.basis = basalt::fixed(120)});
  split.set_first(std::make_unique<basalt::label>("Files"));
  split.set_second(std::make_unique<basalt::label>("Editor"));
  split.set_first_length(basalt::fixed(120));
  split.set_minimums(basalt::fixed(80), basalt::fixed(80));
  split.set_resizable(true);
  host.content().set_cross_alignment(basalt::alignment::stretch);

  host.frame();
  std::println("The divider starts at x = {}.", split.divider_rect().origin.x);
  split.move_divider_by(basalt::splitter::key_step.value);
  std::println("The divider is now at x = {}.", split.divider_rect().origin.x);

  return host.finish(argc, argv);
}
