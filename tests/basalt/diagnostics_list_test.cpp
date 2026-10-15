#include <catch2/catch_test_macros.hpp>

#include "basalt/widgets/diagnostics_list.h"
#include "basalt/widgets/scroll_view.h"
#include "basalt/widgets/stack.h"

#include <string>
#include <vector>

#include "ui_fixture.h"

using namespace basalt;
using namespace basalt::testing;
namespace a11y = basalt::accessibility;

namespace {

auto sample() -> std::vector<diagnostic_entry> {
  return {
      {diagnostic_severity::error, "Missing title", "article.xml:3:1"},
      {diagnostic_severity::warning, "Unused image", "images/a.png"},
      {diagnostic_severity::info, "Archive is unsigned", ""},
  };
}

struct fixture {
  fixture() : f({300, 200}) {
    list = &f.tree.emplace_root<stack>().emplace_child<diagnostics_list>();
    list->set_entries(sample());
    list->set_on_activate([this](std::size_t index, diagnostic_entry const &entry) {
      activated.push_back(index);
      messages.push_back(entry.message);
    });
    f.layout();
  }

  ui f;
  diagnostics_list *list = nullptr;
  std::vector<std::size_t> activated;
  std::vector<std::string> messages;
};

} // namespace

TEST_CASE("each entry is a row stacked in order", "[diagnostics_list]") {
  fixture f;
  REQUIRE(f.list->child_count() == 3);
  REQUIRE(f.list->child(0).bounds().top() < f.list->child(1).bounds().top());
  REQUIRE(f.list->child(1).bounds().bottom() <= f.list->child(2).bounds().top());
}

TEST_CASE("arrow keys move among rows without wrapping", "[diagnostics_list]") {
  fixture f;
  f.f.tap(key::tab);
  REQUIRE(f.list->focused_index() == 0);
  f.f.tap(key::down);
  REQUIRE(f.list->focused_index() == 1);
  f.f.tap(key::end);
  REQUIRE(f.list->focused_index() == 2);
  f.f.tap(key::down);
  REQUIRE(f.list->focused_index() == 2);
  f.f.tap(key::home);
  REQUIRE(f.list->focused_index() == 0);
  REQUIRE(f.f.tree.focus().tab_order().size() == 1);
}

TEST_CASE("clicking or pressing Enter activates a row", "[diagnostics_list]") {
  fixture f;
  f.f.click(f.list->child(1).bounds().center());
  REQUIRE(f.activated == std::vector<std::size_t>{1});
  REQUIRE(f.messages == std::vector<std::string>{"Unused image"});
  f.f.tap(key::up);
  f.f.tap(key::enter);
  REQUIRE(f.activated == std::vector<std::size_t>{1, 0});
}

TEST_CASE("replacing the entries rebuilds the rows and keeps focus in the list",
          "[diagnostics_list]") {
  fixture f;
  f.list->focus_row(2);
  f.list->set_entries({{diagnostic_severity::error, "Only one", ""}});
  f.f.layout();
  REQUIRE(f.list->child_count() == 1);
  REQUIRE(f.list->focused_index() == 0);
  REQUIRE(f.list->entries().size() == 1);
  REQUIRE_FALSE(f.list->focus_row(1));
}

TEST_CASE("rows are named by severity and message", "[diagnostics_list][accessibility]") {
  fixture f;
  f.list->focus_row(1);
  auto const snapshot = f.f.semantics();
  auto const *list = snapshot.find(f.list->id());
  REQUIRE(list->semantics.kind == a11y::role::list);
  REQUIRE(list->children.size() == 3);

  auto const *error = snapshot.find(f.list->child(0).id());
  REQUIRE(error->semantics.kind == a11y::role::list_item);
  REQUIRE(error->semantics.name == "Error: Missing title");
  REQUIRE(error->semantics.description == "article.xml:3:1");
  REQUIRE(error->semantics.position_in_set == 1);
  REQUIRE(error->semantics.set_size == 3);
  REQUIRE(snapshot.find(f.list->child(1).id())->semantics.name == "Warning: Unused image");
  REQUIRE(snapshot.find(f.list->child(2).id())->semantics.name == "Note: Archive is unsigned");
  REQUIRE(snapshot.find(f.list->child(1).id())->semantics.selected);
  REQUIRE(a11y::audit(snapshot).empty());

  REQUIRE(a11y::perform_action(f.f.tree, f.list->child(2).id(), {a11y::action::press}));
  REQUIRE(f.activated == std::vector<std::size_t>{2});
}

TEST_CASE("severity shapes differ so color is not the only cue", "[diagnostics_list][paint]") {
  fixture f;
  auto const commands = f.f.commands();
  // One filled circle for the error, three lines for the warning and one
  // ring for the note.
  REQUIRE(count_commands<fill_rounded_rect_command>(commands) == 1);
  REQUIRE(count_commands<line_command>(commands) == 3);
  REQUIRE(count_commands<stroke_rounded_rect_command>(commands) == 1);
}

TEST_CASE("an empty list says so", "[diagnostics_list]") {
  fixture f;
  f.list->set_entries({});
  f.list->set_empty_message("Nothing to report");
  f.f.layout();
  REQUIRE(f.list->measure({}).height >=
          f.list->resolve(f.f.tree.theme().sizes.dimension_height_element_min));
  REQUIRE(f.list->empty_message() == "Nothing to report");
}

TEST_CASE("focusing a row scrolls it into view", "[diagnostics_list][scroll_view]") {
  ui f({300, 60});
  auto &view = f.tree.emplace_root<scroll_view>();
  auto &list = view.emplace_content<diagnostics_list>();
  std::vector<diagnostic_entry> many;
  for (int i = 0; i < 10; ++i) {
    many.push_back({diagnostic_severity::info, "Note " + std::to_string(i), ""});
  }
  list.set_entries(std::move(many));
  f.layout();

  list.focus_row(9);
  f.layout();
  REQUIRE(view.scroll_offset().y == view.max_scroll_offset().y);
  REQUIRE(view.scroll_offset().y > 0);
}
