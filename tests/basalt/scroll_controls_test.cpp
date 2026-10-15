#include <catch2/catch_test_macros.hpp>

#include "basalt/widgets/scroll_view.h"

#include "leaf.h"
#include "ui_fixture.h"

using namespace basalt;
using namespace basalt::testing;
using namespace std::chrono_literals;
namespace a11y = basalt::accessibility;

namespace {

struct fixture {
  explicit fixture(scroll_axes axes = scroll_axes::vertical) : f({100, 50}) {
    view = &f.tree.emplace_root<scroll_view>(axes);
    content = &view->emplace_content<leaf>(size{300, 200});
    view->set_scrollbars(true);
    f.layout();
  }

  ui f;
  scroll_view *view = nullptr;
  leaf *content = nullptr;
};

auto wheel(point at, point delta, bool precise = true) -> wheel_event {
  return {.position = at, .delta = delta, .precise = precise};
}

} // namespace

TEST_CASE("the wheel scrolls along the scrolling axes", "[scroll_view]") {
  fixture f(scroll_axes::vertical);
  REQUIRE(f.f.send(wheel({10, 10}, {0, 30})));
  f.f.advance(1s);
  REQUIRE(f.view->scroll_offset() == point{0, 30});
  REQUIRE(f.f.send(wheel({10, 10}, {0, -10})));
  f.f.advance(1s);
  REQUIRE(f.view->scroll_offset() == point{0, 20});
  // Nothing scrolls horizontally, so the event is left for ancestors.
  REQUIRE_FALSE(f.f.send(wheel({10, 10}, {20, 0})));
}

TEST_CASE("wheel notches scroll by a fixed step and shift scrolls sideways", "[scroll_view]") {
  fixture f(scroll_axes::both);
  f.f.send(wheel({10, 10}, {0, 1}, false));
  f.f.advance(1s);
  REQUIRE(f.view->scroll_offset() == point{0, 40});
  wheel_event sideways = wheel({10, 10}, {0, 1}, false);
  sideways.modifiers = modifier::shift;
  f.f.send(sideways);
  f.f.advance(1s);
  REQUIRE(f.view->scroll_offset() == point{40, 40});
}

TEST_CASE("the wheel glides: it moves at once, slows, and settles on the destination",
          "[scroll_view][animation]") {
  fixture f(scroll_axes::vertical);
  f.f.send(wheel({10, 10}, {0, 40}));
  // Nothing moves until a frame runs, then the first frame covers the most ground.
  REQUIRE(f.view->scroll_offset().y == 0);
  f.f.advance(17ms);
  float const first = f.view->scroll_offset().y;
  REQUIRE(first > 0);
  f.f.advance(17ms);
  float const second = f.view->scroll_offset().y - first;
  REQUIRE(second > 0);
  REQUIRE(second < first);
  REQUIRE(f.view->scroll_offset().y < 40);
  f.f.advance(1s);
  REQUIRE(f.view->scroll_offset().y == 40);
  // Once settled and painted, no further frames are wanted.
  (void)f.f.tree.take_damage();
  REQUIRE_FALSE(f.f.tree.needs_frame());
}

TEST_CASE("wheel events during a glide extend its destination", "[scroll_view][animation]") {
  fixture f(scroll_axes::vertical);
  f.f.send(wheel({10, 10}, {0, 40}));
  f.f.advance(50ms);
  f.f.send(wheel({10, 10}, {0, 40}));
  f.f.advance(1s);
  REQUIRE(f.view->scroll_offset().y == 80);
}

TEST_CASE("a glide stops at the end of the content", "[scroll_view][animation]") {
  fixture f(scroll_axes::vertical);
  REQUIRE(f.f.send(wheel({10, 10}, {0, 1000})));
  f.f.advance(2s);
  REQUIRE(f.view->scroll_offset().y == f.view->max_scroll_offset().y);
  // At the end the wheel is left for ancestors.
  REQUIRE_FALSE(f.f.send(wheel({10, 10}, {0, 40})));
}

TEST_CASE("a key press ends a glide", "[scroll_view][animation]") {
  fixture f(scroll_axes::vertical);
  f.view->set_focus_policy(focus_policy::tab);
  f.f.tap(key::tab);
  f.f.send(wheel({10, 10}, {0, 100}));
  f.f.advance(30ms);
  REQUIRE(f.f.tap(key::home));
  REQUIRE(f.view->scroll_offset().y == 0);
  f.f.advance(1s);
  REQUIRE(f.view->scroll_offset().y == 0);
}

TEST_CASE("scrollbars appear only while the content overflows", "[scroll_view]") {
  fixture f(scroll_axes::both);
  scroll_bar *const vertical = f.view->scrollbar(axis::vertical);
  scroll_bar *const horizontal = f.view->scrollbar(axis::horizontal);
  REQUIRE(vertical->visible());
  REQUIRE(horizontal->visible());
  REQUIRE(vertical->bounds().right() == 100);
  REQUIRE(horizontal->bounds().bottom() == 50);
  // The bars leave the shared corner free.
  REQUIRE(vertical->bounds().bottom() == horizontal->bounds().top());

  f.content->set_wanted({50, 20});
  f.f.layout();
  REQUIRE_FALSE(vertical->visible());
  REQUIRE_FALSE(horizontal->visible());
}

TEST_CASE("dragging the thumb scrolls proportionally", "[scroll_view]") {
  fixture f(scroll_axes::vertical);
  scroll_bar *const bar = f.view->scrollbar(axis::vertical);
  rect const thumb = bar->thumb_rect();
  REQUIRE(thumb.height() > 0);
  REQUIRE(thumb.height() < bar->bounds().height());

  point const grab = bar->bounds().origin + thumb.center();
  f.f.send(press(grab));
  f.f.send(move(grab + point{0, 1000}, pointer_button::primary));
  REQUIRE(f.view->scroll_offset().y == f.view->max_scroll_offset().y);
  f.f.send(move(grab, pointer_button::primary));
  REQUIRE(f.view->scroll_offset().y == 0);
  f.f.send(release(grab));
}

TEST_CASE("pressing the track pages toward the press", "[scroll_view]") {
  fixture f(scroll_axes::vertical);
  scroll_bar *const bar = f.view->scrollbar(axis::vertical);
  point const below = bar->bounds().origin + point{3, bar->bounds().height() - 2};
  f.f.send(press(below));
  f.f.send(release(below));
  REQUIRE(f.view->scroll_offset().y == 45);
}

TEST_CASE("a focused scroll view scrolls with the keyboard", "[scroll_view]") {
  fixture f(scroll_axes::vertical);
  f.view->set_focus_policy(focus_policy::tab);
  REQUIRE_FALSE(f.f.tap(key::down));
  f.f.tap(key::tab);
  REQUIRE(f.view->has_focus());
  f.f.tap(key::down);
  REQUIRE(f.view->scroll_offset().y == 40);
  f.f.tap(key::page_down);
  // A page is the view less one line, but at least one line.
  REQUIRE(f.view->scroll_offset().y == 80);
  f.f.tap(key::end);
  REQUIRE(f.view->scroll_offset().y == 150);
  f.f.tap(key::up);
  REQUIRE(f.view->scroll_offset().y == 110);
  f.f.tap(key::home);
  REQUIRE(f.view->scroll_offset().y == 0);
}

TEST_CASE("scrollbars follow the offset and expose a range", "[scroll_view][accessibility]") {
  fixture f(scroll_axes::vertical);
  f.view->scroll_by({0, 75});
  scroll_bar *const bar = f.view->scrollbar(axis::vertical);
  REQUIRE(bar->offset() == 75);
  auto const snapshot = f.f.semantics();
  auto const *node = snapshot.find(bar->id());
  REQUIRE(node != nullptr);
  REQUIRE(node->semantics.kind == a11y::role::scroll_bar);
  REQUIRE(node->semantics.numeric_value == 75.0);
  REQUIRE(node->semantics.range->maximum == 150.0);

  REQUIRE(a11y::perform_action(f.f.tree, bar->id(), {a11y::action::decrement}));
  REQUIRE(f.view->scroll_offset().y == 70);
}

TEST_CASE("turning scrollbars off removes them", "[scroll_view]") {
  fixture f(scroll_axes::vertical);
  REQUIRE(f.view->child_count() == 3);
  f.view->set_scrollbars(false);
  REQUIRE(f.view->child_count() == 1);
  REQUIRE(f.view->scrollbar(axis::vertical) == nullptr);
  REQUIRE(f.view->content() == f.content);
}
