// A tooltip shown above a button, as a hover popup. The pointer rests on the button first.

#include <print>

#include "example_host.h"

auto main(int argc, char **argv) -> int {
  auto host = example::example_host{{240, 112}};

  auto &save = host.content().emplace_child<basalt::button>("Save");
  host.content().set_padding(basalt::length_insets::uniform(basalt::fixed(40)));
  host.frame();

  auto const bubble = host.overlays().show_hover_popup(
      std::make_unique<basalt::tooltip>("Save the document"), save.bounds_in_root());
  host.frame();
  std::println("The tooltip is {}.", host.overlays().is_open(bubble) ? "visible" : "hidden");

  return host.finish(argc, argv);
}
