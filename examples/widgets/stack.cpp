// A badge layered over the corner of an avatar.

#include <print>

#include "example_host.h"

auto main(int argc, char **argv) -> int {
  auto host = example::example_host{{80, 80}};

  auto &badged = host.content().emplace_child<basalt::stack>();
  auto &person =
      badged.emplace_child<basalt::avatar>("Margaret Hamilton", basalt::control_size::large);
  person.set_layout(basalt::avatar_layout::graphic_only);
  badged.add(std::make_unique<basalt::chip>("New", basalt::control_size::small),
             basalt::stack_item{.horizontal = basalt::alignment::end,
                                .vertical = basalt::alignment::start});

  host.frame();
  std::println("The stack is {} by {} layout pixels.", badged.bounds().extent.width,
               badged.bounds().extent.height);

  return host.finish(argc, argv);
}
