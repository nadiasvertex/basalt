// A grid of ten thousand colored cells in a scroll view. Only the visible cells are painted.

#include <cstddef>
#include <cstdint>
#include <print>

#include "example_host.h"

namespace {

// A color for each item, so that the cells are easy to tell apart.
auto item_color(std::size_t index) -> basalt::rgba {
  return {static_cast<std::uint8_t>(60 + index * 37 % 160),
          static_cast<std::uint8_t>(90 + index * 53 % 120), std::uint8_t{200}, std::uint8_t{255}};
}

} // namespace

auto main(int argc, char **argv) -> int {
  auto host = example::example_host{{320, 200}};

  auto &view =
      host.content().add(std::make_unique<basalt::scroll_view>(basalt::scroll_axes::vertical),
                         basalt::box_item{.basis = basalt::fixed(168)});
  host.content().set_cross_alignment(basalt::alignment::stretch);
  auto &grid = view.emplace_content<basalt::grid_view>("Colors");
  grid.set_item_count(10'000);
  grid.set_cell_size(48);
  grid.set_gap(8);
  auto painted = std::size_t{0};
  grid.set_cell_painter([&](basalt::render_context &context, std::size_t index, basalt::rect cell) {
    context.fill_rect(cell, item_color(index));
    ++painted;
  });
  grid.set_on_visible_range_changed([](basalt::index_range visible) {
    std::println("Load items {} to {}.", visible.first, visible.last - 1);
  });

  host.frame();
  view.scroll_by({0, 5'000});
  host.frame();
  std::println("The grid has {} columns and {} rows.", grid.columns(), grid.rows());

  auto const under_pointer = grid.index_at({30, 5'030});
  if (under_pointer) {
    std::println("Item {} is at (30, 5030) in the grid.", *under_pointer);
  }

  auto const status = host.finish(argc, argv);
  std::println("{} cells were painted for the screenshot.", painted);
  return status;
}
