// A menu bar. The program opens the File menu, which is built when it opens.

#include <print>

#include "example_host.h"

auto main(int argc, char **argv) -> int {
  auto host = example::example_host{{320, 176}};

  auto &bar = host.content().emplace_child<basalt::menu_bar>();
  bar.add_menu("File", [](basalt::menu &file) {
    file.add_item("new", "New", "Ctrl+N");
    file.add_item("open", "Open", "Ctrl+O");
    file.add_item("quit", "Quit", "Ctrl+Q");
  });
  bar.add_menu("View", [](basalt::menu &view) { view.add_item("wrap", "Wrap lines", "", true); });
  bar.set_on_command([](std::string const &id) { std::println("The command is {}.", id); });

  host.frame();
  bar.open_menu(0);
  host.frame();
  std::println("The menu is {}.", bar.is_open() ? "open" : "closed");

  return host.finish(argc, argv);
}
