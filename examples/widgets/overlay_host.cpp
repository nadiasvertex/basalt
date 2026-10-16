// A popup menu shown in the overlay host, below an anchor rectangle.

#include <print>

#include "example_host.h"

auto main(int argc, char **argv) -> int {
  auto host = example::example_host{{240, 176}};

  auto &more = host.content().emplace_child<basalt::button>("More");
  host.frame();

  auto actions = std::make_unique<basalt::menu>("More actions");
  actions->add_item("rename", "Rename");
  actions->add_item("duplicate", "Duplicate");
  actions->add_item("delete", "Delete");
  auto const popup = host.overlays().show_popup(std::move(actions), more.bounds_in_root(),
                                                [] { std::println("The popup closed."); });
  host.frame();
  std::println("{} overlay(s) are open.", host.overlays().overlay_count());

  auto const status = host.finish(argc, argv);
  host.overlays().dismiss(popup);
  return status;
}
