// Labels in different roles and tones.

#include <print>

#include "example_host.h"

auto main(int argc, char **argv) -> int {
  auto host = example::example_host{{320, 176}};

  auto &title = host.content().emplace_child<basalt::label>("Project status",
                                                            basalt::text_role::heading_medium);
  title.set_heading_level(2);
  host.content()
      .emplace_child<basalt::label>("Last build finished a minute ago.")
      .set_tone(basalt::text_tone::subdued);
  host.content()
      .emplace_child<basalt::label>("3 tests passed")
      .set_tone(basalt::text_tone::success);
  host.content().emplace_child<basalt::label>("1 warning").set_tone(basalt::text_tone::warning);
  host.content().emplace_child<basalt::label>("1 test failed").set_tone(basalt::text_tone::danger);

  host.frame();
  std::println("The title is \"{}\".", title.text());

  return host.finish(argc, argv);
}
