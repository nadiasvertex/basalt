// A menu with a check item. A simulated click activates the Open item.

#include <print>

#include "example_host.h"

auto main(int argc, char **argv) -> int {
  auto host = example::example_host{{200, 192}};

  auto &file = host.content().emplace_child<basalt::menu>("File");
  file.add_item("new", "New", "Ctrl+N");
  file.add_item("open", "Open", "Ctrl+O");
  file.add_separator();
  file.add_item("wrap", "Wrap lines", "", true);
  file.add_item("quit", "Quit", "Ctrl+Q");
  file.set_on_activate([](std::string const &id) { std::println("The command is {}.", id); });

  host.frame();
  host.click(*file.find_item("open"));

  return host.finish(argc, argv);
}
