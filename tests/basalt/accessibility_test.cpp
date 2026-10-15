#include <catch2/catch_test_macros.hpp>

#include "basalt/accessibility/tree.h"
#include "basalt/widget_tree.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace basalt;
namespace a11y = basalt::accessibility;

namespace {

// A widget whose semantics are set by the test.
struct semantic : widget {
  explicit semantic(a11y::role kind = a11y::role::none, std::string name = {}, rect area = {}) {
    details.kind = kind;
    details.name = std::move(name);
    set_bounds(area);
  }

  [[nodiscard]] auto accessibility_info() const -> a11y::info override { return details; }

  a11y::info details;
  std::vector<a11y::action> performed;
  bool accept = true;

protected:
  auto on_accessibility_action(a11y::action_request const &request) -> bool override {
    performed.push_back(request.kind);
    return accept;
  }
};

auto ids_of(a11y::tree const &t) -> std::vector<widget_id> {
  std::vector<widget_id> ids;
  for (auto const &n : t.nodes()) {
    ids.push_back(n.id);
  }
  return ids;
}

struct fixture {
  fixture() {
    tree.set_viewport({400, 300});
    root = &tree.set_root(std::make_unique<semantic>(a11y::role::window, "Editor"));
  }

  auto add(widget &parent, a11y::role kind, std::string name, rect area = {{0, 0}, {50, 40}})
      -> semantic & {
    return parent.emplace_child<semantic>(kind, std::move(name), area);
  }

  widget_tree tree;
  semantic *root;
};

} // namespace

TEST_CASE("an empty widget tree has an empty semantic tree", "[accessibility]") {
  widget_tree tree;
  auto const snapshot = a11y::build(tree);
  CHECK(snapshot.empty());
  CHECK(snapshot.root() == nullptr);
  CHECK_FALSE(snapshot.focused().valid());
}

TEST_CASE("nodes carry role, name, bounds and parentage", "[accessibility]") {
  fixture f;
  auto &toolbar = f.add(*f.root, a11y::role::toolbar, "Tools", {{10, 20}, {200, 48}});
  auto &save = f.add(toolbar, a11y::role::button, "Save", {{5, 4}, {40, 40}});
  save.details.actions.insert(a11y::action::press);

  auto const snapshot = a11y::build(f.tree);
  REQUIRE(snapshot.size() == 3);
  CHECK(snapshot.root()->id == f.root->id());
  CHECK(snapshot.root()->semantics.kind == a11y::role::window);

  auto const *button = snapshot.find(save.id());
  REQUIRE(button != nullptr);
  CHECK(button->semantics.name == "Save");
  CHECK(button->parent == toolbar.id());
  CHECK(button->bounds == rect{{15, 24}, {40, 40}});
  CHECK(snapshot.parent_of(save.id())->id == toolbar.id());
  CHECK(snapshot.find(toolbar.id())->children == std::vector{save.id()});
}

TEST_CASE("widgets without a role are skipped and their children join the parent",
          "[accessibility]") {
  fixture f;
  auto &layout = f.add(*f.root, a11y::role::none, "");
  auto &inner = f.add(layout, a11y::role::none, "");
  auto &label = f.add(inner, a11y::role::label, "Name");
  auto &button = f.add(*f.root, a11y::role::button, "OK");

  auto const snapshot = a11y::build(f.tree);
  CHECK(snapshot.size() == 3);
  CHECK(snapshot.find(layout.id()) == nullptr);
  CHECK(snapshot.find(label.id())->parent == f.root->id());
  CHECK(snapshot.root()->children == std::vector{label.id(), button.id()});
}

TEST_CASE("a root without a role becomes a window", "[accessibility]") {
  widget_tree tree;
  auto &root = tree.set_root(std::make_unique<semantic>());
  auto &child = root.emplace_child<semantic>(a11y::role::label, "Hello");

  auto const snapshot = a11y::build(tree);
  REQUIRE(snapshot.size() == 2);
  CHECK(snapshot.root()->semantics.kind == a11y::role::window);
  CHECK(snapshot.root()->children == std::vector{child.id()});
}

TEST_CASE("hidden subtrees are left out", "[accessibility]") {
  fixture f;
  auto &panel = f.add(*f.root, a11y::role::group, "Panel");
  auto &inside = f.add(panel, a11y::role::label, "Inside");
  panel.set_visible(false);

  auto const snapshot = a11y::build(f.tree);
  CHECK(snapshot.find(panel.id()) == nullptr);
  CHECK(snapshot.find(inside.id()) == nullptr);

  panel.set_visible(true);
  CHECK(a11y::build(f.tree).find(inside.id()) != nullptr);
}

TEST_CASE("reading order follows tree order unless a node overrides it", "[accessibility]") {
  fixture f;
  auto &first = f.add(*f.root, a11y::role::label, "first");
  auto &second = f.add(*f.root, a11y::role::label, "second");
  auto &third = f.add(*f.root, a11y::role::label, "third");

  CHECK(ids_of(a11y::build(f.tree)) ==
        std::vector{f.root->id(), first.id(), second.id(), third.id()});

  first.details.reading_order = 2;
  third.details.reading_order = -1;
  auto const snapshot = a11y::build(f.tree);
  CHECK(snapshot.root()->children == std::vector{third.id(), second.id(), first.id()});
  CHECK(ids_of(snapshot) == std::vector{f.root->id(), third.id(), second.id(), first.id()});
}

TEST_CASE("nodes are listed parents first in reading order", "[accessibility]") {
  fixture f;
  auto &a = f.add(*f.root, a11y::role::group, "a");
  auto &b = f.add(*f.root, a11y::role::group, "b");
  auto &a1 = f.add(a, a11y::role::label, "a1");
  auto &b1 = f.add(b, a11y::role::label, "b1");

  CHECK(ids_of(a11y::build(f.tree)) == std::vector{f.root->id(), a.id(), a1.id(), b.id(), b1.id()});
}

TEST_CASE("focus, focusability and disabled state come from the widget", "[accessibility]") {
  fixture f;
  auto &field = f.add(*f.root, a11y::role::text_field, "Search");
  auto &label = f.add(*f.root, a11y::role::label, "Static");
  field.set_focus_policy(focus_policy::strong);

  auto snapshot = a11y::build(f.tree);
  CHECK(snapshot.find(field.id())->focusable);
  CHECK(snapshot.find(field.id())->supports(a11y::action::focus));
  CHECK_FALSE(snapshot.find(label.id())->focusable);
  CHECK_FALSE(snapshot.find(label.id())->supports(a11y::action::focus));
  CHECK_FALSE(snapshot.focused().valid());

  REQUIRE(field.request_focus());
  snapshot = a11y::build(f.tree);
  CHECK(snapshot.focused() == field.id());
  CHECK(snapshot.find(field.id())->focused);

  // Disabled controls stay focusable and report as disabled, as do their
  // descendants.
  f.root->set_enabled(false);
  snapshot = a11y::build(f.tree);
  CHECK(snapshot.find(field.id())->disabled);
  CHECK(snapshot.find(field.id())->focusable);
  CHECK(snapshot.find(label.id())->disabled);
}

TEST_CASE("focus on a widget without semantics resolves to the nearest node", "[accessibility]") {
  fixture f;
  auto &list = f.add(*f.root, a11y::role::list, "Files");
  auto &plain = f.add(list, a11y::role::none, "");
  plain.set_focus_policy(focus_policy::strong);
  REQUIRE(plain.request_focus());

  CHECK(a11y::build(f.tree).focused() == list.id());
}

TEST_CASE("an active modal marks everything outside it as blocked", "[accessibility]") {
  fixture f;
  auto &page = f.add(*f.root, a11y::role::group, "Page");
  auto &dialog = f.add(*f.root, a11y::role::dialog, "Confirm");
  auto &ok = f.add(dialog, a11y::role::button, "OK");
  ok.set_focus_policy(focus_policy::strong);
  REQUIRE(f.tree.focus().push_modal(dialog));

  auto const snapshot = a11y::build(f.tree);
  CHECK(snapshot.modal() == dialog.id());
  CHECK(snapshot.find(page.id())->blocked);
  CHECK(snapshot.find(f.root->id())->blocked);
  CHECK_FALSE(snapshot.find(dialog.id())->blocked);
  CHECK_FALSE(snapshot.find(ok.id())->blocked);
  CHECK(snapshot.focused() == ok.id());
}

TEST_CASE("diff reports added, removed and changed nodes", "[accessibility]") {
  fixture f;
  auto &keep = f.add(*f.root, a11y::role::label, "keep");
  auto &gone = f.add(*f.root, a11y::role::label, "gone");
  auto const before = a11y::build(f.tree);
  CHECK(a11y::diff(before, a11y::build(f.tree)).empty());

  auto const gone_id = gone.id();
  keep.details.name = "renamed";
  (void)f.root->remove_child(gone);
  auto &fresh = f.add(*f.root, a11y::role::label, "fresh");
  auto const d = a11y::diff(before, a11y::build(f.tree));

  CHECK(d.added == std::vector{fresh.id()});
  CHECK(d.removed == std::vector{gone_id});
  // The root's child list changed too.
  CHECK(std::ranges::find(d.changed, keep.id()) != d.changed.end());
  CHECK(std::ranges::find(d.changed, f.root->id()) != d.changed.end());
  CHECK_FALSE(d.focus_changed);
}

TEST_CASE("diff reports a change of focus", "[accessibility]") {
  fixture f;
  auto &button = f.add(*f.root, a11y::role::button, "Go");
  button.set_focus_policy(focus_policy::strong);
  auto const before = a11y::build(f.tree);
  REQUIRE(button.request_focus());

  auto const d = a11y::diff(before, a11y::build(f.tree));
  CHECK(d.focus_changed);
  CHECK(d.changed == std::vector{button.id()});
}

TEST_CASE("the focus action moves keyboard focus", "[accessibility]") {
  fixture f;
  auto &button = f.add(*f.root, a11y::role::button, "Go");
  button.set_focus_policy(focus_policy::strong);

  CHECK(a11y::perform_action(f.tree, button.id(), {a11y::action::focus}));
  CHECK(button.has_focus());
  CHECK(button.performed.empty());
}

TEST_CASE("actions reach the widget only when it exposes them", "[accessibility]") {
  fixture f;
  auto &button = f.add(*f.root, a11y::role::button, "Go");
  button.details.actions.insert(a11y::action::press);

  CHECK(a11y::perform_action(f.tree, button.id(), {a11y::action::press}));
  CHECK(button.performed == std::vector{a11y::action::press});

  CHECK_FALSE(a11y::perform_action(f.tree, button.id(), {a11y::action::increment}));
  CHECK(button.performed.size() == 1);

  button.accept = false;
  CHECK_FALSE(a11y::perform_action(f.tree, button.id(), {a11y::action::press}));
}

TEST_CASE("actions are refused for disabled, hidden, blocked and unknown widgets",
          "[accessibility]") {
  fixture f;
  auto &button = f.add(*f.root, a11y::role::button, "Go");
  button.details.actions.insert(a11y::action::press);

  button.set_enabled(false);
  CHECK_FALSE(a11y::perform_action(f.tree, button.id(), {a11y::action::press}));
  button.set_enabled(true);

  button.set_visible(false);
  CHECK_FALSE(a11y::perform_action(f.tree, button.id(), {a11y::action::press}));
  button.set_visible(true);

  auto &dialog = f.add(*f.root, a11y::role::dialog, "Modal");
  REQUIRE(f.tree.focus().push_modal(dialog));
  CHECK_FALSE(a11y::perform_action(f.tree, button.id(), {a11y::action::press}));
  REQUIRE(f.tree.focus().pop_modal(dialog));

  CHECK_FALSE(a11y::perform_action(f.tree, widget_id{~0ULL}, {a11y::action::press}));
  CHECK(button.performed.empty());
}

TEST_CASE("audit finds controls that are unnamed or unreachable", "[accessibility]") {
  fixture f;
  auto &unnamed = f.add(*f.root, a11y::role::button, "");
  unnamed.set_focus_policy(focus_policy::strong);
  unnamed.details.actions.insert(a11y::action::press);

  auto &unreachable = f.add(*f.root, a11y::role::button, "Hidden from Tab");
  unreachable.details.actions.insert(a11y::action::press);

  auto &inert = f.add(*f.root, a11y::role::button, "Inert", {});
  inert.set_focus_policy(focus_policy::strong);

  auto &dangling = f.add(*f.root, a11y::role::label, "Hint");
  dangling.details.described_by.push_back(widget_id{~0ULL});

  auto &good = f.add(*f.root, a11y::role::button, "Fine");
  good.set_focus_policy(focus_policy::strong);
  good.details.actions.insert(a11y::action::press);

  auto const issues = a11y::audit(a11y::build(f.tree));
  auto has = [&](widget const &w, a11y::issue_kind kind) {
    return std::ranges::find(issues, a11y::issue{w.id(), kind}) != issues.end();
  };
  CHECK(has(unnamed, a11y::issue_kind::missing_name));
  CHECK(has(unreachable, a11y::issue_kind::interactive_not_focusable));
  CHECK(has(inert, a11y::issue_kind::missing_actions));
  CHECK(has(inert, a11y::issue_kind::empty_bounds));
  CHECK(has(dangling, a11y::issue_kind::dangling_reference));
  CHECK(std::ranges::none_of(issues, [&](auto const &i) { return i.id == good.id(); }));
}

TEST_CASE("a name supplied by a label satisfies the audit", "[accessibility]") {
  fixture f;
  auto &label = f.add(*f.root, a11y::role::label, "Search");
  auto &field = f.add(*f.root, a11y::role::text_field, "");
  field.set_focus_policy(focus_policy::strong);
  field.details.actions.insert(a11y::action::set_value);
  field.details.labelled_by.push_back(label.id());

  CHECK(a11y::audit(a11y::build(f.tree)).empty());
}

TEST_CASE("role names and classification", "[accessibility]") {
  CHECK(a11y::to_string(a11y::role::check_box) == "check_box");
  CHECK(a11y::is_interactive(a11y::role::tab));
  CHECK_FALSE(a11y::is_interactive(a11y::role::label));
  CHECK(a11y::requires_name(a11y::role::dialog));
  CHECK_FALSE(a11y::requires_name(a11y::role::group));
}
