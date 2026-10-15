#include <catch2/catch_test_macros.hpp>

#include "basalt/widgets/palette.h"
#include "basalt/widgets/stack.h"

#include <string>
#include <vector>

#include "ui_fixture.h"

using namespace basalt;
using namespace basalt::testing;

namespace {

auto sample() -> std::vector<palette_entry> {
  return {{"file.open", "Open…", "⌘O"}, {"file.save", "Save", "⌘S  :w"}, {"file.quit", "Quit", ""}};
}

struct fixture {
  fixture() : f({600, 400}) {
    shown = &f.tree.emplace_root<stack>().emplace_child<palette>();
    shown->set_entries(sample());
    shown->set_on_choose([this](std::string const &id) { chosen.push_back(id); });
    shown->set_on_query_change([this](std::string const &text) { queries.push_back(text); });
    f.layout();
    shown->focus_query();
  }

  ui f;
  palette *shown = nullptr;
  std::vector<std::string> chosen;
  std::vector<std::string> queries;
};

} // namespace

TEST_CASE("the palette lists one row per entry below the search box", "[palette]") {
  fixture f;
  REQUIRE(f.shown->child_count() == 2);
  REQUIRE(f.shown->child(1).child_count() == 3);
  REQUIRE(f.shown->selected_index() == 0);
}

TEST_CASE("arrow keys move the selection and wrap", "[palette]") {
  fixture f;
  f.f.tap(key::down);
  REQUIRE(f.shown->selected_index() == 1);
  f.f.tap(key::up);
  f.f.tap(key::up);
  REQUIRE(f.shown->selected_index() == 2);
  f.f.tap(key::down);
  REQUIRE(f.shown->selected_index() == 0);
}

TEST_CASE("Enter chooses the selected entry", "[palette]") {
  fixture f;
  f.f.tap(key::down);
  f.f.tap(key::enter);
  REQUIRE(f.chosen == std::vector<std::string>{"file.save"});
}

TEST_CASE("typing reports the query", "[palette]") {
  fixture f;
  f.f.type("sa");
  REQUIRE_FALSE(f.queries.empty());
  REQUIRE(f.queries.back() == "sa");
  REQUIRE(f.shown->query() == "sa");
}

TEST_CASE("clicking a row chooses it", "[palette]") {
  fixture f;
  f.f.click(f.shown->child(1).child(2).bounds_in_root().center());
  REQUIRE(f.chosen == std::vector<std::string>{"file.quit"});
}

TEST_CASE("an empty list shows a message and chooses nothing", "[palette]") {
  fixture f;
  f.shown->set_entries({});
  f.f.layout();
  REQUIRE(f.shown->child(1).child_count() == 1);
  f.f.tap(key::enter);
  REQUIRE(f.chosen.empty());
}

TEST_CASE("only a window of rows is shown and follows the selection", "[palette]") {
  fixture f;
  std::vector<palette_entry> many;
  for (int i = 0; i < 25; ++i) {
    many.push_back({"c" + std::to_string(i), "Command " + std::to_string(i), ""});
  }
  f.shown->set_entries(many);
  REQUIRE(f.shown->child(1).child_count() == palette::visible_rows);
  f.f.tap(key::up);
  REQUIRE(f.shown->selected_index() == 24);
  REQUIRE(f.shown->child(1).child_count() == palette::visible_rows);
  f.f.tap(key::enter);
  REQUIRE(f.chosen == std::vector<std::string>{"c24"});
}
