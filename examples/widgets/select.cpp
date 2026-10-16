// A single-choice select. The program opens the list and moves the choice with the keyboard.

#include <print>

#include "example_host.h"

auto main(int argc, char **argv) -> int {
  auto host = example::example_host{{240, 216}};

  auto &language = host.content().emplace_child<basalt::select>("Language");
  language.set_placeholder("Choose one");
  for (auto const *name : {"C++", "C#", "Go", "TypeScript"}) {
    language.add_option(name);
  }
  language.set_helper_text("Used for new projects");
  language.set_on_change([&language] {
    std::println("{} is chosen.", language.option_label(language.chosen_indices().front()));
  });

  host.frame();
  host.click(language);
  host.press_key(basalt::key::down);
  host.press_key(basalt::key::down);
  host.press_key(basalt::key::enter);
  host.click(language);
  std::println("The list is {}.", language.is_open() ? "open" : "closed");

  return host.finish(argc, argv);
}
