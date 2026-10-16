// A single-line field that receives typed text, and a read-only multi-line field.

#include <print>

#include "example_host.h"

auto main(int argc, char **argv) -> int {
  auto host = example::example_host{{320, 128}};

  auto &name = host.content().emplace_child<basalt::text_field>();
  name.set_placeholder("Your name");
  name.set_accessible_name("Name");
  name.set_on_change([](std::string const &text) { std::println("The text is \"{}\".", text); });
  name.set_on_submit([](std::string const &text) { std::println("Submitted \"{}\".", text); });
  auto &notes = host.content().emplace_child<basalt::text_field>(
      "A multi-line field wraps at its width.", true);
  notes.set_accessible_name("Notes");
  notes.set_read_only(true);
  host.content().set_cross_alignment(basalt::alignment::stretch);

  host.frame();
  host.click(name);
  host.type("Ada");
  host.press_key(basalt::key::enter);

  return host.finish(argc, argv);
}
