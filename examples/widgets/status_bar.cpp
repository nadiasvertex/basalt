// A status bar. The first section grows and pushes the others to the right.

#include <print>

#include "example_host.h"

auto main(int argc, char **argv) -> int {
  auto host = example::example_host{{480, 64}};

  auto &status = host.content().emplace_child<basalt::status_bar>();
  status.add_section("message", "Ready", 1);
  status.add_section("position", "Ln 1, Col 1");
  status.add_section("encoding", "UTF-8");
  host.content().set_cross_alignment(basalt::alignment::stretch);

  status.set_section_text("message", "Saved");
  status.set_section_text("position", "Ln 12, Col 5");
  host.frame();
  std::println("The message section reads \"{}\".", status.section("message")->text());

  return host.finish(argc, argv);
}
