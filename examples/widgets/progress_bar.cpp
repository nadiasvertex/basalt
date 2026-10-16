// A determinate bar and an indeterminate bar that moves each time `tick` runs.

#include <print>

#include "example_host.h"

auto main(int argc, char **argv) -> int {
  auto host = example::example_host{{320, 64}};

  auto &upload = host.content().emplace_child<basalt::progress_bar>(std::optional<float>{0.6F});
  auto &waiting = host.content().emplace_child<basalt::progress_bar>();
  host.content().set_cross_alignment(basalt::alignment::stretch);

  upload.set_fraction(0.75F);
  waiting.tick(0.3F);
  host.frame();
  std::println("The upload is {:.0f} percent done.", *upload.fraction() * 100);
  std::println("The second bar is {}.", waiting.fraction() ? "determinate" : "indeterminate");

  return host.finish(argc, argv);
}
