#include <catch2/catch_test_macros.hpp>

#include "basalt/widgets/tree_view.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "ui_fixture.h"

using namespace basalt;
using namespace basalt::testing;
namespace a11y = basalt::accessibility;

namespace {

// A tree kept in pre-order, as an application keeps the data behind a tree view.
struct outline {
  struct node {
    std::string text;
    std::uint32_t depth = 0;
    bool expanded = false;
  };

  [[nodiscard]] auto has_children(std::size_t i) const -> bool {
    return i + 1 < nodes.size() && nodes[i + 1].depth > nodes[i].depth;
  }

  // The node behind each shown row.
  [[nodiscard]] auto shown() const -> std::vector<std::size_t> {
    std::vector<std::size_t> out;
    for (std::size_t i = 0; i < nodes.size();) {
      out.push_back(i);
      std::size_t next = i + 1;
      if (!nodes[i].expanded) {
        while (next < nodes.size() && nodes[next].depth > nodes[i].depth) {
          ++next;
        }
      }
      i = next;
    }
    return out;
  }

  [[nodiscard]] auto rows() -> tree_rows {
    return {
        .count = [this] { return shown().size(); },
        .row =
            [this](std::size_t index) {
              std::size_t const i = shown()[index];
              return tree_row{.text = nodes[i].text,
                              .depth = nodes[i].depth,
                              .expandable = has_children(i),
                              .expanded = nodes[i].expanded};
            },
        .depth = {},
    };
  }

  std::vector<node> nodes;
};

auto sample() -> outline {
  return {{
      {"Alpha", 0},
      {"Alpha one", 1},
      {"Alpha two", 1},
      {"Beta", 0},
      {"Bravo", 0},
      {"Charlie", 0},
  }};
}

struct fixture {
  explicit fixture(outline data = sample(), basalt::size viewport = {300, 200},
                   scale_factors scale = {})
      : f(viewport, scale), model(std::move(data)) {
    view = &f.tree.emplace_root<tree_view>("Contents");
    view->set_rows(model.rows());
    view->set_on_select([this](std::size_t index) { selected.push_back(index); });
    view->set_on_activate([this](std::size_t index) { activated.push_back(index); });
    view->set_on_expand([this](std::size_t index, bool expand) {
      expanded.emplace_back(index, expand);
      model.nodes[model.shown()[index]].expanded = expand;
      view->rows_changed();
    });
    f.layout();
  }

  [[nodiscard]] auto row_center(std::size_t index) const -> point {
    return view->row_area(index).center();
  }
  [[nodiscard]] auto row_id(std::size_t index) const -> widget_id {
    auto const *const row = view->realized_row(index);
    REQUIRE(row != nullptr);
    return row->id();
  }

  ui f;
  outline model;
  tree_view *view = nullptr;
  std::vector<std::size_t> selected;
  std::vector<std::size_t> activated;
  std::vector<std::pair<std::size_t, bool>> expanded;
};

auto many(std::size_t count) -> outline {
  outline out;
  for (std::size_t i = 0; i < count; ++i) {
    out.nodes.push_back({"Row " + std::to_string(i), 0});
  }
  return out;
}

auto typed(std::string text, std::chrono::milliseconds at) -> text_input_event {
  return {std::move(text), at};
}

} // namespace

TEST_CASE("only the rows in view are realized", "[tree_view]") {
  fixture f(many(1000), {300, 120});
  std::size_t const in_view = static_cast<std::size_t>(120 / f.view->row_height()) + 1;
  REQUIRE(f.view->row_count() == 1000);
  REQUIRE(f.view->realized_row_count() >= in_view - 1);
  REQUIRE(f.view->realized_row_count() <= in_view);

  f.view->scroll_to_row(999);
  f.f.layout();
  REQUIRE(f.view->realized_row(999) != nullptr);
  REQUIRE(f.view->realized_row(500) == nullptr);
  // The first row keeps the Tab stop while nothing is selected.
  REQUIRE(f.view->realized_row(0) != nullptr);
  REQUIRE(f.view->realized_row_count() <= in_view + 1);
  REQUIRE(f.view->row_area(999).bottom() <= 120.5f);
}

TEST_CASE("the selected row stays realized when it scrolls out of view", "[tree_view]") {
  fixture f(many(1000), {300, 120});
  f.f.tap(key::tab);
  f.f.tap(key::down);
  f.f.tap(key::down);
  REQUIRE(f.view->selected() == 1);
  widget_id const focused = f.row_id(1);
  f.view->scroll_to_row(900);
  f.f.layout();
  REQUIRE(f.f.tree.focus().focused()->id() == focused);
  REQUIRE(f.view->realized_row(1) != nullptr);
  f.f.tap(key::down);
  REQUIRE(f.view->selected() == 2);
  REQUIRE(f.view->row_area(2).top() >= 0);
}

TEST_CASE("the rows form one Tab stop", "[tree_view]") {
  fixture f;
  REQUIRE(f.f.tree.focus().tab_order().size() == 1);
  f.view->select(3);
  f.f.layout();
  auto const order = f.f.tree.focus().tab_order();
  REQUIRE(order.size() == 1);
  REQUIRE(order.front()->id() == f.row_id(3));
}

TEST_CASE("arrow keys move, expand and collapse", "[tree_view]") {
  fixture f;
  f.f.tap(key::tab);
  f.f.tap(key::down);
  REQUIRE(f.view->selected() == 0);

  f.f.tap(key::right);
  REQUIRE(f.expanded == std::vector<std::pair<std::size_t, bool>>{{0, true}});
  REQUIRE(f.view->row_count() == 6);
  REQUIRE(f.view->selected() == 0);

  f.f.tap(key::right);
  REQUIRE(f.view->selected() == 1);
  f.f.tap(key::down);
  REQUIRE(f.view->selected() == 2);
  f.f.tap(key::left);
  REQUIRE(f.view->selected() == 0);
  f.f.tap(key::left);
  REQUIRE(f.expanded.back() == std::pair<std::size_t, bool>{0, false});
  REQUIRE(f.view->row_count() == 4);
  REQUIRE(f.view->selected() == 0);

  f.f.tap(key::end);
  REQUIRE(f.view->selected() == 3);
  f.f.tap(key::down);
  REQUIRE(f.view->selected() == 3);
  f.f.tap(key::home);
  REQUIRE(f.view->selected() == 0);
  REQUIRE(f.selected == std::vector<std::size_t>{0, 1, 2, 0, 3, 0});
  REQUIRE(f.f.tree.focus().focused()->id() == f.row_id(0));
}

TEST_CASE("Page Down and Page Up move by a page of rows", "[tree_view]") {
  fixture f(many(100), {300, 120});
  auto const page = static_cast<std::size_t>(120 / f.view->row_height());
  f.f.tap(key::tab);
  f.f.tap(key::page_down);
  REQUIRE(f.view->selected() == page);
  f.f.tap(key::page_up);
  REQUIRE(f.view->selected() == 0);
}

TEST_CASE("the selection follows a collapse and an expand above it", "[tree_view]") {
  fixture f;
  f.model.nodes[0].expanded = true;
  f.view->rows_changed();
  f.f.layout();

  f.view->select(4);
  REQUIRE(a11y::perform_action(f.f.tree, f.row_id(0), {a11y::action::collapse}));
  f.f.layout();
  REQUIRE(f.view->selected() == 2);
  REQUIRE(a11y::perform_action(f.f.tree, f.row_id(0), {a11y::action::expand}));
  f.f.layout();
  REQUIRE(f.view->selected() == 4);

  f.view->select(2);
  REQUIRE(a11y::perform_action(f.f.tree, f.row_id(0), {a11y::action::collapse}));
  f.f.layout();
  // The selected row was hidden, so its parent is selected.
  REQUIRE(f.view->selected() == 0);
  REQUIRE(f.selected.empty());
}

TEST_CASE("a click selects a row and a double click activates it", "[tree_view]") {
  fixture f;
  f.f.click(f.row_center(3));
  REQUIRE(f.view->selected() == 3);
  REQUIRE(f.selected == std::vector<std::size_t>{3});
  REQUIRE(f.activated.empty());
  REQUIRE(f.f.tree.focus().focused()->id() == f.row_id(3));

  pointer_event second = press(f.row_center(3));
  second.click_count = 2;
  f.f.send(second);
  f.f.send(release(f.row_center(3)));
  REQUIRE(f.activated == std::vector<std::size_t>{3});

  f.f.tap(key::enter);
  REQUIRE(f.activated == std::vector<std::size_t>{3, 3});
}

TEST_CASE("a click on the chevron expands the row", "[tree_view]") {
  fixture f;
  rect const area = f.view->row_area(0);
  auto const &sizes = f.f.tree.theme().sizes;
  float const mark_center =
      f.view->resolve(sizes.inset_horizontal_list_item) + f.view->resolve(sizes.dimension_icon) / 2;
  f.f.click({mark_center, area.center().y});
  REQUIRE(f.expanded == std::vector<std::pair<std::size_t, bool>>{{0, true}});
  REQUIRE(f.view->selected() == 0);

  f.f.click(f.row_center(0));
  REQUIRE(f.expanded.size() == 1);
}

TEST_CASE("typing moves to the next row that starts with the text", "[tree_view]") {
  using std::chrono::milliseconds;
  fixture f;
  f.f.tap(key::tab);
  f.f.send(typed("b", milliseconds{100}));
  REQUIRE(f.view->selected() == 1);
  f.f.send(typed("R", milliseconds{200}));
  REQUIRE(f.view->selected() == 2);

  f.f.send(typed("c", milliseconds{2000}));
  REQUIRE(f.view->selected() == 3);
  // After a pause a search starts below the selected row and wraps.
  f.f.send(typed("b", milliseconds{4000}));
  REQUIRE(f.view->selected() == 1);
  f.f.send(typed("b", milliseconds{6000}));
  REQUIRE(f.view->selected() == 2);
}

TEST_CASE("select does not report the change or take focus", "[tree_view]") {
  fixture f;
  f.view->select(2);
  f.f.layout();
  REQUIRE(f.view->selected() == 2);
  REQUIRE(f.selected.empty());
  REQUIRE(f.f.tree.focus().focused() == nullptr);

  f.view->select(std::nullopt);
  REQUIRE_FALSE(f.view->selected().has_value());
  f.view->select(40);
  REQUIRE_FALSE(f.view->selected().has_value());
}

TEST_CASE("the selection is clamped when rows go away", "[tree_view]") {
  fixture f;
  f.view->select(3);
  f.model.nodes.resize(2);
  f.view->rows_changed();
  f.f.layout();
  REQUIRE(f.view->row_count() == 1);
  REQUIRE(f.view->selected() == 0);
  REQUIRE(f.view->realized_row_count() == 1);

  f.model.nodes.clear();
  f.view->rows_changed();
  f.f.layout();
  REQUIRE_FALSE(f.view->selected().has_value());
  REQUIRE(f.view->realized_row_count() == 0);
}

TEST_CASE("realized rows show the data after it changes", "[tree_view][accessibility]") {
  fixture f;
  f.model.nodes[3].text = "Gamma";
  f.view->rows_changed();
  f.f.layout();
  REQUIRE(f.f.semantics().find(f.row_id(1))->semantics.name == "Gamma");
}

TEST_CASE("rows are tree items with level, state and position", "[tree_view][accessibility]") {
  fixture f;
  f.model.nodes[0].expanded = true;
  f.view->set_rows(f.model.rows());
  f.view->select(1);
  f.f.layout();

  auto const snapshot = f.f.semantics();
  auto const *tree = snapshot.find(f.view->id());
  REQUIRE(tree->semantics.kind == a11y::role::tree);
  REQUIRE(tree->semantics.name == "Contents");

  auto const *alpha = snapshot.find(f.row_id(0));
  REQUIRE(alpha->semantics.kind == a11y::role::tree_item);
  REQUIRE(alpha->semantics.name == "Alpha");
  REQUIRE(alpha->semantics.level == 1);
  REQUIRE(alpha->semantics.expanded == true);
  REQUIRE(alpha->supports(a11y::action::collapse));
  REQUIRE_FALSE(alpha->supports(a11y::action::expand));
  REQUIRE_FALSE(alpha->semantics.selected);

  auto const *child = snapshot.find(f.row_id(1));
  REQUIRE(child->semantics.level == 2);
  REQUIRE_FALSE(child->semantics.expanded.has_value());
  REQUIRE(child->semantics.selected);
  REQUIRE(a11y::audit(snapshot).empty());

  REQUIRE(a11y::perform_action(f.f.tree, f.row_id(5), {a11y::action::press}));
  REQUIRE(f.activated == std::vector<std::size_t>{5});
}

TEST_CASE("the application's set position reaches assistive technology",
          "[tree_view][accessibility]") {
  fixture f;
  f.view->set_rows({
      .count = [] { return std::size_t{1}; },
      .row =
          [](std::size_t) { return tree_row{.text = "Only", .position_in_set = 3, .set_size = 9}; },
      .depth = {},
  });
  f.f.layout();
  auto const snapshot = f.f.semantics();
  auto const *row = snapshot.find(f.row_id(0));
  REQUIRE(row->semantics.position_in_set == 3);
  REQUIRE(row->semantics.set_size == 9);
}

TEST_CASE("the selected row is filled with the accent color", "[tree_view][paint]") {
  fixture f;
  auto const selected_fills = [&] {
    auto const commands = f.f.commands();
    return std::ranges::count_if(commands, [&](paint_command const &c) {
      auto const *fill = std::get_if<fill_rect_command>(&c);
      return fill != nullptr && fill->color == f.f.tree.theme().colors.background_accent_subdued;
    });
  };
  REQUIRE(selected_fills() == 0);
  f.view->select(1);
  f.f.layout();
  REQUIRE(selected_fills() == 1);
}

TEST_CASE("rows scale with the display", "[tree_view]") {
  fixture normal;
  fixture doubled(sample(), {300, 200}, scale_factors{.zoom = 2});
  REQUIRE(doubled.view->row_height() == normal.view->row_height() * 2);
  REQUIRE(doubled.view->realized_row_count() == 4);
}
