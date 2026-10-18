// A tree of folders and files. The program owns the expanded state, and the tree asks it for the
// rows it shows.

#include <cstddef>
#include <cstdint>
#include <print>
#include <string>
#include <vector>

#include "example_host.h"

namespace {

struct entry {
  std::string name;
  std::uint32_t depth = 0;
  bool expanded = false;
};

// The entries in pre-order.
std::vector<entry> entries{
    {"src", 0, true}, {"main.cpp", 1}, {"tree.cpp", 1},
    {"docs", 0},      {"guide.md", 1}, {"README.md", 0},
};

auto has_children(std::size_t i) -> bool {
  return i + 1 < entries.size() && entries[i + 1].depth > entries[i].depth;
}

// The entry behind each row. A collapsed entry hides its descendants.
auto shown() -> std::vector<std::size_t> {
  std::vector<std::size_t> rows;
  for (std::size_t i = 0; i < entries.size();) {
    rows.push_back(i);
    std::size_t next = i + 1;
    while (!entries[i].expanded && next < entries.size() &&
           entries[next].depth > entries[i].depth) {
      ++next;
    }
    i = next;
  }
  return rows;
}

} // namespace

auto main(int argc, char **argv) -> int {
  auto host = example::example_host{{240, 180}};
  host.content().set_cross_alignment(basalt::alignment::stretch);

  auto &tree = host.content().emplace_child<basalt::tree_view>("Files");
  tree.set_rows({
      .count = [] { return shown().size(); },
      .row =
          [](std::size_t index) {
            std::size_t const i = shown()[index];
            return basalt::tree_row{.text = entries[i].name,
                                    .depth = entries[i].depth,
                                    .expandable = has_children(i),
                                    .expanded = entries[i].expanded};
          },
      .depth = {},
  });
  tree.set_on_expand([&tree](std::size_t index, bool expand) {
    entries[shown()[index]].expanded = expand;
    tree.rows_changed();
    std::println("{} {}.", expand ? "Expanded" : "Collapsed", entries[shown()[index]].name);
  });
  tree.set_on_select(
      [](std::size_t index) { std::println("Selected {}.", entries[shown()[index]].name); });
  tree.set_on_activate(
      [](std::size_t index) { std::println("Opened {}.", entries[shown()[index]].name); });

  host.frame();
  host.press_key(basalt::key::tab);
  host.press_key(basalt::key::end);
  host.press_key(basalt::key::up);
  host.press_key(basalt::key::right);
  host.press_key(basalt::key::down);
  host.press_key(basalt::key::enter);

  return host.finish(argc, argv);
}
