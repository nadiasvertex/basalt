// A command palette. The owner filters the entries when the query changes.

#include <algorithm>
#include <cctype>
#include <print>
#include <string>
#include <vector>

#include "example_host.h"

namespace {

auto commands() -> std::vector<basalt::palette_entry> {
  return {
      {.id = "theme", .label = "Toggle theme", .detail = "Ctrl+T", .category = "View"},
      {.id = "dialog", .label = "Show dialog", .category = "Window"},
      {.id = "save", .label = "Save file", .detail = "Ctrl+S", .category = "File"},
      {.id = "save_all", .label = "Save all files", .category = "File"},
  };
}

auto matching(std::string query) -> std::vector<basalt::palette_entry> {
  std::ranges::transform(query, query.begin(),
                         [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  auto result = std::vector<basalt::palette_entry>{};
  for (auto &entry : commands()) {
    auto label = entry.label;
    std::ranges::transform(label, label.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (label.contains(query)) {
      result.push_back(std::move(entry));
    }
  }
  return result;
}

} // namespace

auto main(int argc, char **argv) -> int {
  auto host = example::example_host{{640, 240}};

  auto shown = std::make_unique<basalt::palette>();
  auto &palette = *shown;
  palette.set_placeholder("Type a command");
  palette.set_entries(commands());
  palette.set_on_query_change(
      [&palette](std::string const &query) { palette.set_entries(matching(query)); });
  palette.set_on_choose([](std::string const &id) { std::println("The command is {}.", id); });
  host.overlays().show_dialog(std::move(shown));

  host.frame();
  palette.focus_query();
  host.type("save");
  std::println("{} entries match.", palette.entries().size());
  palette.choose_selected();

  return host.finish(argc, argv);
}
