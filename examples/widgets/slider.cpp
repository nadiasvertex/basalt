// A slider with a label and value text. Keys change the value, and the handlers report it.

#include <format>
#include <print>

#include "example_host.h"

auto main(int argc, char **argv) -> int {
  auto host = example::example_host{{280, 72}};

  auto &blur = host.content().emplace_child<basalt::slider>(0.0F, 100.0F, 1.0F, 40.0F);
  blur.set_label("Blur threshold");
  blur.set_value_formatter([](float v) { return std::format("{:.0f}", v); });
  blur.set_on_change([](float v) { std::println("Preview at {:.0f}.", v); });
  blur.set_on_commit([](float v) { std::println("Chose {:.0f}.", v); });
  host.content().set_cross_alignment(basalt::alignment::stretch);

  host.frame();
  blur.request_focus(basalt::focus_reason::keyboard);
  host.press_key(basalt::key::page_up);
  host.press_key(basalt::key::end);

  return host.finish(argc, argv);
}
