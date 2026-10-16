// A popover that opens when its button trigger is clicked.

#include <print>

#include "example_host.h"

auto main(int argc, char **argv) -> int {
  auto host = example::example_host{{280, 120}};

  auto &details = host.content().emplace_child<basalt::popover>(basalt::popover_trigger::click);
  auto &trigger = details.emplace_trigger<basalt::button>("Details");
  trigger.set_on_press([&details] { details.toggle(); });
  details.set_text("The build ran on three machines.");
  details.set_on_visibility_changed(
      [](bool open) { std::println("The popover {}.", open ? "opened" : "closed"); });

  host.frame();
  host.click(trigger);

  return host.finish(argc, argv);
}
