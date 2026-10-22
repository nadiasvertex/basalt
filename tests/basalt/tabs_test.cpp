#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "basalt/widgets/box.h"
#include "basalt/widgets/menu.h"
#include "basalt/widgets/overlay_host.h"
#include "basalt/widgets/stack.h"
#include "basalt/widgets/tabs.h"

#include <algorithm>
#include <string>
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

namespace {

// A bar of twelve tabs in a window that fits about four, optionally under an overlay host.
struct overflowing_bar {
  explicit overflowing_bar(bool with_host = false) {
    if (with_host) {
      host = &f.tree.emplace_root<overlay_host>();
      auto &content = host->set_content(std::make_unique<box>(axis::vertical));
      content.set_cross_alignment(alignment::start);
      bar = &content.emplace_child<tab_bar>("Documents");
    } else {
      bar = &f.tree.emplace_root<stack>().emplace_child<tab_bar>("Documents");
    }
    for (int i = 0; i < 12; ++i) {
      bar->add_tab("Document " + std::to_string(i));
    }
    bar->set_on_select([this](std::size_t index) { selections.push_back(index); });
    f.layout();
  }

  // The overflow control with the accessible name, or null.
  [[nodiscard]] auto control(std::string const &name) const -> widget * {
    for (widget &c : bar->children()) {
      if (c.accessibility_info().name == name &&
          c.accessibility_info().kind == a11y::role::button) {
        return &c;
      }
    }
    return nullptr;
  }

  [[nodiscard]] auto shown(std::string const &name) const -> bool {
    widget const *const c = control(name);
    return c != nullptr && c->visible();
  }

  // The x positions of the strips that fade the bar's background into the tabs.
  [[nodiscard]] auto fade_strips() -> std::vector<float> {
    auto const background = f.tree.theme().colors.background_default;
    std::vector<float> positions;
    for (auto const &c : f.commands()) {
      if (auto const *r = std::get_if<fill_rect_command>(&c);
          r != nullptr && r->color.r == background.r && r->color.g == background.g &&
          r->color.b == background.b && r->color.a > 0 && r->color.a < 255) {
        positions.push_back(r->bounds.left());
      }
    }
    return positions;
  }

  ui f{{400, 300}};
  overlay_host *host = nullptr;
  tab_bar *bar = nullptr;
  std::vector<std::size_t> selections;
};

} // namespace

TEST_CASE("tabs that fit show no overflow controls or fade", "[tabs][overflow]") {
  fixture f;
  REQUIRE_FALSE(f.bar->overflowing());
  for (widget &c : f.bar->children()) {
    if (c.accessibility_info().kind == a11y::role::button) {
      REQUIRE_FALSE(c.visible());
    }
  }
  REQUIRE(f.bar->tab_count() == 3);
  auto const accent = f.f.tree.theme().colors.background_default;
  for (auto const &c : f.f.commands()) {
    if (auto const *r = std::get_if<fill_rect_command>(&c)) {
      REQUIRE_FALSE((r->color.r == accent.r && r->color.a > 0 && r->color.a < 255));
    }
  }
}

TEST_CASE("overflowing tabs show both scroll buttons, disabled at their ends", "[tabs][overflow]") {
  overflowing_bar o;
  REQUIRE(o.bar->overflowing());
  REQUIRE(o.shown("Scroll tabs left"));
  REQUIRE(o.shown("Scroll tabs right"));
  REQUIRE_FALSE(o.shown("All tabs"));
  REQUIRE_FALSE(o.control("Scroll tabs left")->enabled());
  REQUIRE(o.control("Scroll tabs right")->enabled());

  o.bar->set_scroll_offset(o.bar->max_scroll_offset());
  o.f.layout();
  REQUIRE(o.control("Scroll tabs left")->enabled());
  REQUIRE_FALSE(o.control("Scroll tabs right")->enabled());
}

TEST_CASE("tabs are never drawn under the scroll buttons", "[tabs][overflow]") {
  overflowing_bar o;
  float const left = o.control("Scroll tabs left")->bounds().right();
  float const right = o.control("Scroll tabs right")->bounds().left();
  REQUIRE(o.bar->tab_at(0).bounds().left() == left);
  o.bar->set_scroll_offset(o.bar->max_scroll_offset());
  o.f.layout();
  REQUIRE(o.bar->tab_at(11).bounds().right() == right);
}

TEST_CASE("pressing the trailing button shows the first partly hidden tab", "[tabs][overflow]") {
  overflowing_bar o;
  float const edge = o.control("Scroll tabs right")->bounds().left();
  std::size_t hidden = 0;
  while (o.bar->tab_at(hidden).bounds().right() <= edge) {
    ++hidden;
  }
  REQUIRE(hidden > 0);
  REQUIRE(o.bar->tab_at(hidden).bounds().left() < edge);

  o.f.click(o.control("Scroll tabs right")->bounds().center());
  REQUIRE(o.bar->tab_at(hidden).bounds().right() == edge);
  REQUIRE(o.bar->tab_at(hidden + 1).bounds().left() >= edge - 1);

  o.f.click(o.control("Scroll tabs left")->bounds().center());
  REQUIRE(o.bar->scroll_offset() == 0);
}

TEST_CASE("holding a scroll button keeps scrolling", "[tabs][overflow]") {
  overflowing_bar o;
  auto const at = o.control("Scroll tabs right")->bounds().center();
  o.f.send(press(at));
  float const after_press = o.bar->scroll_offset();
  REQUIRE(after_press > 0);
  o.f.advance(std::chrono::milliseconds{300});
  REQUIRE(o.bar->scroll_offset() == after_press);
  o.f.advance(std::chrono::milliseconds{400});
  REQUIRE(o.bar->scroll_offset() > after_press);
  o.f.send(release(at));
  float const released = o.bar->scroll_offset();
  o.f.advance(std::chrono::milliseconds{500});
  REQUIRE(o.bar->scroll_offset() == released);
}

TEST_CASE("the fade is painted only at an edge with tabs past it", "[tabs][overflow][paint]") {
  overflowing_bar o;
  float const middle = o.bar->bounds().width() / 2;
  auto const only_after = [&](std::vector<float> const &strips) {
    return !strips.empty() && std::ranges::all_of(strips, [&](float x) { return x > middle; });
  };
  auto const only_before = [&](std::vector<float> const &strips) {
    return !strips.empty() && std::ranges::all_of(strips, [&](float x) { return x < middle; });
  };
  REQUIRE(only_after(o.fade_strips()));

  o.bar->set_scroll_offset(o.bar->max_scroll_offset() / 2);
  o.f.layout();
  auto const both = o.fade_strips();
  REQUIRE(std::ranges::any_of(both, [&](float x) { return x < middle; }));
  REQUIRE(std::ranges::any_of(both, [&](float x) { return x > middle; }));

  o.bar->set_scroll_offset(o.bar->max_scroll_offset());
  o.f.layout();
  REQUIRE(only_before(o.fade_strips()));
}

TEST_CASE("the menu lists every tab and checks the selected one", "[tabs][overflow][menu]") {
  overflowing_bar o{true};
  REQUIRE(o.shown("All tabs"));
  o.bar->select(2);
  o.f.layout();
  o.f.click(o.control("All tabs")->bounds().center());
  REQUIRE(o.bar->tab_menu_open());

  std::vector<menu_item *> items;
  auto visit = [&](this auto const &self, widget &w) -> void {
    for (widget &c : w.children()) {
      if (auto *item = dynamic_cast<menu_item *>(&c)) {
        items.push_back(item);
      }
      self(c);
    }
  };
  visit(*o.host);
  REQUIRE(items.size() == 12);
  for (std::size_t i = 0; i < items.size(); ++i) {
    REQUIRE(items[i]->label() == "Document " + std::to_string(i));
    REQUIRE(items[i]->checked() == (i == 2));
  }

  o.selections.clear();
  REQUIRE(items[10]->activate());
  o.f.layout();
  REQUIRE_FALSE(o.bar->tab_menu_open());
  REQUIRE(o.bar->selected_index() == 10);
  REQUIRE(o.selections == std::vector<std::size_t>{10});
  REQUIRE(o.bar->tab_at(10).bounds().right() <= o.control("Scroll tabs right")->bounds().left());
  REQUIRE(o.bar->tab_at(10).bounds().left() >= o.control("Scroll tabs left")->bounds().right());
}

TEST_CASE("Down on a tab opens the menu", "[tabs][overflow][menu]") {
  overflowing_bar o{true};
  o.f.tap(key::tab);
  o.f.tap(key::down);
  REQUIRE(o.bar->tab_menu_open());
  o.f.tap(key::escape);
  REQUIRE_FALSE(o.bar->tab_menu_open());
}

TEST_CASE("without an overlay host there is no menu button", "[tabs][overflow][menu]") {
  overflowing_bar o;
  REQUIRE(o.bar->overflowing());
  REQUIRE_FALSE(o.shown("All tabs"));
  REQUIRE_FALSE(o.bar->open_tab_menu());
}

TEST_CASE("the overflow controls expose button semantics", "[tabs][overflow][accessibility]") {
  overflowing_bar o{true};
  auto const snapshot = o.f.semantics();
  for (auto const *name : {"Scroll tabs left", "Scroll tabs right", "All tabs"}) {
    auto const *node = snapshot.find(o.control(name)->id());
    REQUIRE(node != nullptr);
    REQUIRE(node->semantics.kind == a11y::role::button);
  }
  REQUIRE(snapshot.find(o.bar->tab_at(0).id())->semantics.set_size == 12);
  REQUIRE(a11y::audit(snapshot).empty());
}

TEST_CASE("clearing the bar and adding tabs again keeps the controls working", "[tabs][overflow]") {
  overflowing_bar o;
  o.bar->clear_children();
  o.f.layout();
  REQUIRE(o.bar->tab_count() == 0);
  for (int i = 0; i < 12; ++i) {
    o.bar->add_tab("Again " + std::to_string(i));
  }
  o.f.layout();
  REQUIRE(o.bar->tab_count() == 12);
  REQUIRE(o.bar->overflowing());
  REQUIRE(o.shown("Scroll tabs right"));
}

namespace {

struct segmented_bar {
  explicit segmented_bar(int tabs) {
    bar = &f.tree.emplace_root<stack>().emplace_child<tab_bar>("Mode");
    bar->set_style(tab_bar_style::segmented);
    for (int i = 0; i < tabs; ++i) {
      bar->add_tab("Option " + std::to_string(i));
    }
    f.layout();
  }

  ui f{{400, 300}};
  tab_bar *bar = nullptr;
};

} // namespace

TEST_CASE("a segmented bar gives its tabs equal widths", "[tabs][segmented]") {
  segmented_bar s{4};
  float const width = s.bar->tab_at(0).bounds().width();
  for (std::size_t i = 1; i < 4; ++i) {
    // Edges snap to device pixels, so widths may differ by one.
    REQUIRE(s.bar->tab_at(i).bounds().width() == Catch::Approx(width).margin(1));
  }
  REQUIRE(width > 80);
  REQUIRE(s.bar->tab_at(3).bounds().right() <= s.bar->bounds().width());
}

TEST_CASE("a segmented bar never scrolls", "[tabs][segmented]") {
  segmented_bar s{20};
  REQUIRE_FALSE(s.bar->overflowing());
  REQUIRE(s.bar->max_scroll_offset() == 0);
  REQUIRE(s.bar->tab_at(19).bounds().right() <= s.bar->bounds().width());
  for (widget &c : s.bar->children()) {
    if (c.accessibility_info().kind == a11y::role::button) {
      REQUIRE_FALSE(c.visible());
    }
  }
}

TEST_CASE("a segmented bar selects with Left and Right", "[tabs][segmented]") {
  segmented_bar s{3};
  s.f.tap(key::tab);
  s.f.tap(key::right);
  REQUIRE(s.bar->selected_index() == 1);
  s.f.tap(key::left);
  s.f.tap(key::left);
  REQUIRE(s.bar->selected_index() == 2);
}
