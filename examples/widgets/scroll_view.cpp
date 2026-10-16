// A scroll view with scroll bars over a column of labels.

#include <format>
#include <print>

#include "example_host.h"

auto main(int argc, char **argv) -> int {
  auto host = example::example_host{{240, 168}};

  auto &view =
      host.content().add(std::make_unique<basalt::scroll_view>(basalt::scroll_axes::vertical),
                         basalt::box_item{.basis = basalt::fixed(136)});
  view.set_scrollbars(true);
  auto &rows = view.emplace_content<basalt::box>(basalt::axis::vertical);
  for (auto number = 1; number <= 20; ++number) {
    rows.emplace_child<basalt::label>(std::format("Row {}", number));
  }
  host.content().set_cross_alignment(basalt::alignment::stretch);

  host.frame();
  view.scroll_by({0, 60});
  host.frame();
  std::println("The view shows content from y = {}.", view.scroll_offset().y);
  std::println("The content is {:.0f} layout pixels tall.", view.content_size().height);

  return host.finish(argc, argv);
}
