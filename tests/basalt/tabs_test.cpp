#include <catch2/catch_test_macros.hpp>

#include "basalt/widgets/stack.h"
#include "basalt/widgets/tabs.h"

#include <vector>

#include "ui_fixture.h"

using namespace basalt;
using namespace basalt::testing;
namespace a11y = basalt::accessibility;

namespace {

struct fixture {
  fixture() {
    auto &root = f.tree.emplace_root<stack>();
    bar = &root.emplace_child<tab_bar>("Documents");
    first = &bar->add_tab("One");
    second = &bar->add_tab("Two");
    third = &bar->add_tab("Three");
    bar->set_on_select([this](std::size_t index) { selections.push_back(index); });
    f.layout();
  }

  ui f;
  tab_bar *bar = nullptr;
  tab *first = nullptr;
  tab *second = nullptr;
  tab *third = nullptr;
  std::vector<std::size_t> selections;
};

} // namespace

TEST_CASE("the first tab is selected and tabs sit side by side", "[tabs]") {
  fixture f;
  REQUIRE(f.bar->selected_index() == 0);
  REQUIRE(f.first->selected());
  REQUIRE(f.first->bounds().right() <= f.second->bounds().left());
  REQUIRE(f.first->measure({}).height ==
          f.first->resolve(f.f.tree.theme().sizes.dimension_height_tab));
}

TEST_CASE("clicking a tab selects it once", "[tabs]") {
  fixture f;
  f.f.click(f.third->bounds().center());
  REQUIRE(f.bar->selected_index() == 2);
  REQUIRE_FALSE(f.first->selected());
  REQUIRE(f.selections == std::vector<std::size_t>{2});
  f.f.click(f.third->bounds().center());
  REQUIRE(f.selections.size() == 1);
}

TEST_CASE("arrow keys move focus and selection together", "[tabs]") {
  fixture f;
  f.f.tap(key::tab);
  REQUIRE(f.first->has_focus());
  f.f.tap(key::right);
  REQUIRE(f.second->has_focus());
  REQUIRE(f.bar->selected_index() == 1);
  f.f.tap(key::end);
  REQUIRE(f.bar->selected_index() == 2);
  f.f.tap(key::right);
  REQUIRE(f.bar->selected_index() == 0);
  f.f.tap(key::left);
  REQUIRE(f.bar->selected_index() == 2);
  REQUIRE(f.selections == std::vector<std::size_t>{1, 2, 0, 2});
}

TEST_CASE("Enter and Space select the focused tab but a held key does not", "[tabs]") {
  fixture f;
  f.f.tap(key::tab);
  f.f.tap(key::right);
  REQUIRE(f.second->has_focus());
  f.bar->select(0);
  f.selections.clear();

  f.f.send(key_event{.code = key::enter, .repeat = true});
  REQUIRE(f.bar->selected_index() == 0);
  REQUIRE(f.selections.empty());

  f.f.tap(key::enter);
  REQUIRE(f.bar->selected_index() == 1);
  f.bar->select(0);
  f.f.tap(key::space);
  REQUIRE(f.bar->selected_index() == 1);
}

TEST_CASE("Tab enters the tab list at the selected tab", "[tabs]") {
  fixture f;
  f.bar->select(1);
  REQUIRE(f.f.tree.focus().tab_order().size() == 1);
  REQUIRE(f.f.tree.focus().tab_order().front() == f.second);
}

TEST_CASE("select rejects an index out of range", "[tabs]") {
  fixture f;
  REQUIRE_FALSE(f.bar->select(3));
  REQUIRE(f.bar->selected_index() == 0);
}

TEST_CASE("tabs expose tab list semantics", "[tabs][accessibility]") {
  fixture f;
  auto const snapshot = f.f.semantics();
  auto const *list = snapshot.find(f.bar->id());
  REQUIRE(list->semantics.kind == a11y::role::tab_list);
  REQUIRE(list->semantics.name == "Documents");
  auto const *node = snapshot.find(f.second->id());
  REQUIRE(node->semantics.kind == a11y::role::tab);
  REQUIRE(node->semantics.position_in_set == 2);
  REQUIRE(node->semantics.set_size == 3);
  REQUIRE_FALSE(node->semantics.selected);
  REQUIRE(snapshot.find(f.first->id())->semantics.selected);
  REQUIRE(a11y::audit(snapshot).empty());

  REQUIRE(a11y::perform_action(f.f.tree, f.third->id(), {a11y::action::select}));
  REQUIRE(f.bar->selected_index() == 2);
}

TEST_CASE("the selected tab is underlined", "[tabs][paint]") {
  fixture f;
  auto const accent = f.f.tree.theme().colors.border_accent;
  int underlines = 0;
  for (auto const &c : f.f.commands()) {
    if (auto const *r = std::get_if<fill_rect_command>(&c); r != nullptr && r->color == accent) {
      ++underlines;
    }
  }
  REQUIRE(underlines == 1);
}

namespace {

struct crowded {
  crowded() {
    auto &root = f.tree.emplace_root<stack>();
    bar = &root.emplace_child<tab_bar>("Documents");
    for (int i = 0; i < 12; ++i) {
      bar->add_tab("Document number " + std::to_string(i));
    }
    f.layout();
  }

  ui f{{400, 300}};
  tab_bar *bar = nullptr;
};

} // namespace

TEST_CASE("tabs that do not fit make the row scrollable", "[tabs][scroll]") {
  crowded c;
  REQUIRE(c.bar->max_scroll_offset() > 0);
  REQUIRE(c.bar->scroll_offset() == 0);
  REQUIRE(c.bar->tab_at(0).bounds().width() >= 96);
}

TEST_CASE("a vertical wheel scrolls the row sideways", "[tabs][scroll]") {
  crowded c;
  auto const before = c.bar->tab_at(3).bounds().left();
  REQUIRE(c.f.send(wheel_event{.position = {10, 10}, .delta = {0, 50}, .precise = true}));
  REQUIRE(c.bar->scroll_offset() > 0);
  REQUIRE(c.bar->tab_at(3).bounds().left() == before - c.bar->scroll_offset());
  c.f.send(wheel_event{.position = {10, 10}, .delta = {0, -5000}, .precise = true});
  REQUIRE(c.bar->scroll_offset() == 0);
}

TEST_CASE("selecting a tab scrolls it into view", "[tabs][scroll]") {
  crowded c;
  c.bar->select(11);
  c.f.layout();
  auto const area = c.bar->tab_at(11).bounds();
  REQUIRE(area.right() <= c.bar->bounds().width());
  REQUIRE(c.bar->scroll_offset() > 0);
  c.bar->select(0);
  c.f.layout();
  REQUIRE(c.bar->scroll_offset() == 0);
}
