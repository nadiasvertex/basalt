// A list of problems. Keyboard focus moves to a row and Enter activates it.

#include <print>

#include "example_host.h"

auto main(int argc, char **argv) -> int {
  auto host = example::example_host{{360, 128}};

  auto &problems = host.content().emplace_child<basalt::diagnostics_list>();
  problems.set_entries({
      {basalt::diagnostic_severity::error, "Missing closing tag", "document.xml:12:5"},
      {basalt::diagnostic_severity::warning, "Unused style", "styles.xml:3:1"},
      {basalt::diagnostic_severity::info, "Document saved", ""},
  });
  problems.set_on_activate([](std::size_t, basalt::diagnostic_entry const &entry) {
    std::println("Go to {}.", entry.location);
  });

  host.frame();
  problems.focus_row(1);
  host.press_key(basalt::key::enter);

  return host.finish(argc, argv);
}
