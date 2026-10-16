// A horizontal scroll bar. A press on the track beside the thumb pages toward the press.

#include <print>

#include "example_host.h"

auto main(int argc, char **argv) -> int {
  auto host = example::example_host{{320, 44}};

  auto &bar = host.content().add(std::make_unique<basalt::scroll_bar>(basalt::axis::horizontal),
                                 basalt::box_item{.basis = basalt::scroll_bar::default_thickness});
  host.content().set_cross_alignment(basalt::alignment::stretch);
  bar.set_metrics(1000, 250, 0);
  bar.set_on_scroll([&bar](float offset) {
    std::println("The bar asks for offset {}.", offset);
    bar.set_metrics(1000, 250, offset);
  });

  host.frame();
  auto const thumb = bar.thumb_rect();
  host.click_at({bar.bounds_in_root().right() - 8, bar.bounds_in_root().center().y});
  std::println("The thumb is {} layout pixels wide.", thumb.extent.width);

  return host.finish(argc, argv);
}
