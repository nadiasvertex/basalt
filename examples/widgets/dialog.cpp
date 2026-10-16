// A confirmation dialog in an overlay host. A simulated choice presses the second button.

#include <print>

#include "example_host.h"

auto main(int argc, char **argv) -> int {
  auto host = example::example_host{{480, 200}};

  auto shown = std::make_unique<basalt::dialog>(
      "Discard changes?", "The document has changes that are not saved.",
      std::vector<basalt::dialog_button>{{"Cancel"}, {"Discard", basalt::button_variant::danger}},
      0);
  auto &confirm = *shown;
  confirm.set_on_choice([](std::size_t index) { std::println("Button {} was chosen.", index); });
  host.overlays().show_dialog(std::move(shown));

  host.frame();
  confirm.choose(1);

  return host.finish(argc, argv);
}
