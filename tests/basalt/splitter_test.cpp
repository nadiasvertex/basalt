#include <catch2/catch_test_macros.hpp>

#include "basalt/widgets/split_view.h"

#include "leaf.h"
#include "ui_fixture.h"

using namespace basalt;
using namespace basalt::testing;
namespace a11y = basalt::accessibility;

namespace {

struct fixture {
  explicit fixture(axis direction = axis::horizontal) : f({201, 201}) {
    split = &f.tree.emplace_root<split_view>(direction);
    first = &split->set_first(std::make_unique<leaf>());
    second = &split->set_second(std::make_unique<leaf>());
    split->set_resizable(true);
    f.layout();
    grip = split->handle();
  }

  ui f;
  split_view *split = nullptr;
  leaf *first = nullptr;
  leaf *second = nullptr;
  splitter *grip = nullptr;
};

} // namespace

TEST_CASE("the splitter is a wide grab area centred on the divider", "[splitter]") {
  fixture f;
  REQUIRE(f.grip != nullptr);
  REQUIRE(f.split->child(f.split->child_count() - 1).id() == f.grip->id());
  rect const divider = f.split->divider_rect();
  REQUIRE(f.grip->bounds().width() >= f.grip->resolve(split_view::minimum_grab_width));
  REQUIRE(f.grip->bounds().left() < divider.left());
  REQUIRE(f.grip->bounds().right() > divider.right());
  REQUIRE(f.grip->bounds().height() == 201);
  // The grab area wins over the panes beneath it.
  REQUIRE(f.f.router.hit_test(divider.center()) == f.grip);
  REQUIRE(f.f.router.hit_test({10, 10}) == f.first);
}

TEST_CASE("dragging moves the divider with the pointer", "[splitter]") {
  fixture f;
  point const start = f.split->divider_rect().center();
  f.f.send(press(start));
  f.f.send(move(start + point{30, 0}, pointer_button::primary));
  REQUIRE(f.first->bounds().width() == 130);
  f.f.send(move(start + point{-60, 0}, pointer_button::primary));
  REQUIRE(f.first->bounds().width() == 40);
  f.f.send(release(start + point{-60, 0}));
  f.f.send(move(start, pointer_button::none));
  REQUIRE(f.first->bounds().width() == 40);
}

TEST_CASE("the pane minimums limit dragging", "[splitter]") {
  fixture f;
  f.split->set_minimums(fixed(50), fixed(50));
  point const start = f.split->divider_rect().center();
  f.f.send(press(start));
  f.f.send(move({2000, 5}, pointer_button::primary));
  REQUIRE(f.second->bounds().width() == 50);
  f.f.send(move({-2000, 5}, pointer_button::primary));
  REQUIRE(f.first->bounds().width() == 50);
  f.f.send(release({-2000, 5}));
}

TEST_CASE("the splitter can be resized with the keyboard", "[splitter]") {
  fixture f;
  f.grip->request_focus();
  float const before = f.first->bounds().width();
  f.f.tap(key::right);
  REQUIRE(f.first->bounds().width() == before + 16);
  f.f.tap(key::left);
  f.f.tap(key::left);
  REQUIRE(f.first->bounds().width() == before - 16);
  f.f.tap(key::end);
  REQUIRE(f.second->bounds().width() == 0);
  f.f.tap(key::home);
  REQUIRE(f.first->bounds().width() == 0);
}

TEST_CASE("a stacked split uses the vertical arrows", "[splitter]") {
  fixture f(axis::vertical);
  f.grip->request_focus();
  float const before = f.first->bounds().height();
  f.f.tap(key::down);
  REQUIRE(f.first->bounds().height() == before + 16);
  REQUIRE_FALSE(f.f.tap(key::right));
}

TEST_CASE("the splitter is hidden when only one pane is shown", "[splitter]") {
  fixture f;
  f.second->set_visible(false);
  f.f.layout();
  REQUIRE_FALSE(f.grip->visible());
  f.second->set_visible(true);
  f.f.layout();
  REQUIRE(f.grip->visible());
}

TEST_CASE("panes set after resizing stay behind the splitter", "[splitter]") {
  fixture f;
  auto &replacement = f.split->set_second(std::make_unique<leaf>());
  REQUIRE(f.split->child_count() == 3);
  REQUIRE(f.split->child(2).id() == f.grip->id());
  REQUIRE(f.split->second() == &replacement);
  f.split->set_resizable(false);
  REQUIRE(f.split->child_count() == 2);
  REQUIRE(f.split->handle() == nullptr);
}

TEST_CASE("the splitter reports its position to assistive technology",
          "[splitter][accessibility]") {
  fixture f;
  auto const snapshot = f.f.semantics();
  auto const *node = snapshot.find(f.grip->id());
  REQUIRE(node != nullptr);
  REQUIRE(node->semantics.kind == a11y::role::splitter);
  REQUIRE(node->semantics.orientation == a11y::orientation::vertical);
  REQUIRE(node->semantics.numeric_value == static_cast<double>(f.split->divider_rect().left()));
  REQUIRE(node->focusable);
  REQUIRE(a11y::audit(snapshot).empty());

  float const before = f.first->bounds().width();
  REQUIRE(a11y::perform_action(f.f.tree, f.grip->id(), {a11y::action::increment}));
  f.f.layout();
  REQUIRE(f.first->bounds().width() == before + 16);
}

TEST_CASE("a split without a splitter draws its own divider", "[splitter][paint]") {
  fixture f;
  f.split->set_resizable(false);
  f.f.layout();
  REQUIRE(count_commands<fill_rect_command>(f.f.commands()) == 1);
}
