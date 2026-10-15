#include <catch2/catch_test_macros.hpp>

#include "basalt/widgets/box.h"
#include "basalt/widgets/menu.h"
#include "basalt/widgets/overlay_host.h"
#include "basalt/widgets/select.h"

#include <memory>
#include <string>
#include <vector>

#include "input_probe.h"
#include "ui_fixture.h"

using namespace basalt;
using namespace basalt::testing;
namespace a11y = basalt::accessibility;

namespace {

struct fixture {
  explicit fixture(select_mode mode = select_mode::single) {
    host = &f.tree.emplace_root<overlay_host>();
    auto &content = host->set_content(std::make_unique<box>(axis::vertical));
    content.set_cross_alignment(alignment::start);
    field = &content.emplace_child<basalt::select>("Fruit");
    field->set_mode(mode);
    field->set_placeholder("Pick one");
    field->add_option("Apple");
    field->add_option("Banana");
    field->add_option("Cherry");
    field->set_on_change([this] { ++changes; });
    f.layout();
  }

  // The popup's item for the option at `index`, or null while closed.
  [[nodiscard]] auto popup_items() const -> std::vector<std::string> {
    std::vector<std::string> labels;
    visit(*host, labels);
    return labels;
  }

  static auto visit(widget const &w, std::vector<std::string> &labels) -> void {
    for (widget const &c : w.children()) {
      if (auto const *item = dynamic_cast<menu_item const *>(&c)) {
        labels.push_back(item->label());
      }
      visit(c, labels);
    }
  }

  [[nodiscard]] auto item(std::string const &label) const -> menu_item * {
    return find(*host, label);
  }

  static auto find(widget const &w, std::string const &label) -> menu_item * {
    for (widget &c : w.children()) {
      if (auto *item = dynamic_cast<menu_item *>(&c); item != nullptr && item->label() == label) {
        return item;
      }
      if (auto *found = find(c, label)) {
        return found;
      }
    }
    return nullptr;
  }

  ui f;
  overlay_host *host = nullptr;
  basalt::select *field = nullptr;
  int changes = 0;
};

} // namespace

TEST_CASE("a select opens its list on click and Enter", "[select]") {
  fixture f;
  REQUIRE_FALSE(f.field->is_open());
  f.f.click(f.field->bounds().center());
  REQUIRE(f.field->is_open());
  REQUIRE(f.popup_items() == std::vector<std::string>{"Apple", "Banana", "Cherry"});
  f.f.tap(key::escape);
  REQUIRE_FALSE(f.field->is_open());

  f.field->request_focus();
  f.f.tap(key::enter);
  REQUIRE(f.field->is_open());
}

TEST_CASE("choosing in the single mode sets the value and closes", "[select]") {
  fixture f;
  f.field->open();
  REQUIRE(f.item("Banana")->activate());
  REQUIRE(f.field->chosen_indices() == std::vector<std::size_t>{1});
  REQUIRE(f.changes == 1);
  REQUIRE_FALSE(f.field->is_open());
  REQUIRE(f.f.semantics().find(f.field->id())->semantics.value == "Banana");

  f.field->open();
  f.item("Cherry")->activate();
  REQUIRE(f.field->chosen_indices() == std::vector<std::size_t>{2});
}

TEST_CASE("the multiple mode stays open and counts the choice", "[select]") {
  fixture f{select_mode::multiple};
  f.field->open();
  f.item("Apple")->activate();
  f.item("Cherry")->activate();
  REQUIRE(f.field->is_open());
  REQUIRE(f.field->chosen_indices() == std::vector<std::size_t>{0, 2});
  REQUIRE(f.f.semantics().find(f.field->id())->semantics.value == "2 selected");

  f.item("Apple")->activate();
  REQUIRE(f.field->chosen_indices() == std::vector<std::size_t>{2});
  REQUIRE(f.changes == 3);
}

TEST_CASE("chosen options come first when the list reopens", "[select]") {
  fixture f{select_mode::multiple};
  f.field->choose(2);
  f.field->open();
  REQUIRE(f.popup_items() == std::vector<std::string>{"Cherry", "Apple", "Banana"});
}

TEST_CASE("Delete clears a multiple choice", "[select]") {
  fixture f{select_mode::multiple};
  f.field->choose(0);
  f.field->choose(1);
  f.field->request_focus();
  REQUIRE(f.f.tap(key::delete_forward));
  REQUIRE(f.field->chosen_indices().empty());
  REQUIRE(f.changes == 1);
  REQUIRE_FALSE(f.f.tap(key::delete_forward));
}

TEST_CASE("the clear button empties the choice without opening the list", "[select]") {
  fixture f{select_mode::multiple};
  f.field->choose(0);
  f.f.layout();
  rect const b = f.field->bounds_in_root();
  // The clear mark sits left of the chevron at the field's right edge.
  float const x = b.right() - 6 - 24 - 6;
  f.f.click({x, b.top() + 15});
  REQUIRE(f.field->chosen_indices().empty());
  REQUIRE_FALSE(f.field->is_open());
}

TEST_CASE("a disabled select does not open", "[select]") {
  fixture f;
  f.field->set_enabled(false);
  REQUIRE_FALSE(f.field->open());
  REQUIRE_FALSE(f.field->is_open());
}

TEST_CASE("selects expose combo box semantics", "[select][accessibility]") {
  fixture f;
  f.field->set_helper_text("Required");
  f.field->set_error(true);
  auto snapshot = f.f.semantics();
  auto const *node = snapshot.find(f.field->id());
  REQUIRE(node->semantics.kind == a11y::role::combo_box);
  REQUIRE(node->semantics.name == "Fruit");
  REQUIRE(node->semantics.description == "Required");
  REQUIRE(node->semantics.invalid);
  REQUIRE(node->semantics.expanded == false);
  REQUIRE(a11y::audit(snapshot).empty());

  REQUIRE(a11y::perform_action(f.f.tree, f.field->id(), {a11y::action::press}));
  REQUIRE(f.f.semantics().find(f.field->id())->semantics.expanded == true);
}

TEST_CASE("choose replaces the choice in the single mode and rejects bad indices", "[select]") {
  fixture f;
  REQUIRE(f.field->choose(0));
  REQUIRE(f.field->choose(2));
  REQUIRE(f.field->chosen_indices() == std::vector<std::size_t>{2});
  REQUIRE_FALSE(f.field->choose(3));
  REQUIRE(f.changes == 0);
}
