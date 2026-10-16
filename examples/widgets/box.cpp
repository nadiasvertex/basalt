// A row in which the middle child takes the spare width.

#include <print>

#include "example_host.h"

auto main(int argc, char **argv) -> int {
  auto host = example::example_host{{400, 72}};

  auto &row = host.content().add(std::make_unique<basalt::box>(basalt::axis::horizontal),
                                 basalt::box_item{.grow = 1});
  row.set_gap(basalt::fixed(8));
  row.set_cross_alignment(basalt::alignment::center);
  auto &back = row.emplace_child<basalt::button>("Back");
  auto &title = row.add(std::make_unique<basalt::label>("Settings"), basalt::box_item{.grow = 1});
  auto &next = row.emplace_child<basalt::button>("Next");
  title.set_tone(basalt::text_tone::subdued);

  host.content().set_cross_alignment(basalt::alignment::stretch);
  host.frame();
  std::println("Back is {} wide, the title is {} wide, and Next is {} wide.",
               back.bounds().extent.width, title.bounds().extent.width, next.bounds().extent.width);

  return host.finish(argc, argv);
}
