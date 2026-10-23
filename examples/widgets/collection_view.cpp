// A grid of 10,000 items. The factory runs only for the items in view, and keys move the current item.

#include <memory>
#include <print>

#include "example_host.h"

auto main(int argc, char **argv) -> int {
  auto host = example::example_host{{360, 240}};

  auto &grid = host.content().emplace_child<basalt::collection_view>("Photos");
  host.content().set_cross_alignment(basalt::alignment::stretch);
  grid.set_cell_size(basalt::fixed(80), basalt::fixed(80));
  grid.set_gap(basalt::fixed(8));
  grid.set_cell_factory([](std::size_t index, std::unique_ptr<basalt::widget> recycled) {
    auto tile = recycled != nullptr ? std::unique_ptr<basalt::image_tile>(
                                          static_cast<basalt::image_tile *>(recycled.release()))
                                    : std::make_unique<basalt::image_tile>();
    tile->set_caption(std::format("IMG_{:04}", index));
    return tile;
  });
  grid.set_on_visible_change([](basalt::index_range range) {
    std::println("Items {} to {} are in view.", range.first, range.last - 1);
  });
  grid.set_on_activate([](std::size_t index) { std::println("Opened item {}.", index); });
  grid.set_item_count(10'000);

  host.frame();
  grid.request_focus(basalt::focus_reason::keyboard);
  host.press_key(basalt::key::down);
  host.press_key(basalt::key::down);
  host.press_key(basalt::key::enter);
  grid.scroll_to_item(5'000);
  host.frame();
  grid.scroll_to_item(0);
  host.frame();

  return host.finish(argc, argv);
}
