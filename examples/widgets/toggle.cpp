// Two toggles. A simulated click switches Bluetooth on.

#include <print>

#include "example_host.h"

auto main(int argc, char **argv) -> int {
  auto host = example::example_host{{200, 96}};

  auto &wifi = host.content().emplace_child<basalt::toggle>("Wi-Fi", true);
  wifi.set_status_text("Off", "On");
  auto &bluetooth = host.content().emplace_child<basalt::toggle>("Bluetooth");
  bluetooth.set_status_text("Off", "On");
  bluetooth.set_on_change([](bool on) { std::println("Bluetooth is {}.", on ? "on" : "off"); });

  host.frame();
  host.click(bluetooth);

  return host.finish(argc, argv);
}
