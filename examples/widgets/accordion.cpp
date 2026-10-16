// An accordion where opening one item closes the others.

#include <print>

#include "example_host.h"

auto main(int argc, char **argv) -> int {
  auto host = example::example_host{{320, 176}};

  auto &faq = host.content().emplace_child<basalt::accordion>("Questions");
  faq.set_appearance(basalt::accordion_appearance::outlined);
  faq.set_exclusive(true);
  faq.add_item("What is Basalt?", std::make_unique<basalt::label>("A C++ widget library."));
  faq.add_item("Does it open windows?", std::make_unique<basalt::label>("No. The host does."));
  faq.add_item("Which renderer does it use?", std::make_unique<basalt::label>("Any renderer."));
  faq.item(0).set_expanded(true);
  faq.item(1).set_on_toggle([](bool expanded) {
    std::println("The second item is now {}.", expanded ? "open" : "closed");
  });

  host.content().set_cross_alignment(basalt::alignment::stretch);
  host.frame();
  host.click(faq.item(1).header());
  std::println("The first item is {}.", faq.item(0).expanded() ? "open" : "closed");

  return host.finish(argc, argv);
}
