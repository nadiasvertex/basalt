#include <catch2/catch_test_macros.hpp>

#include "basalt/widgets/box.h"
#include "basalt/widgets/text_field.h"

#include <string>
#include <vector>

#include "../text/font_support.h"
#include "ui_fixture.h"

using namespace basalt;
using namespace basalt::testing;
namespace a11y = basalt::accessibility;

namespace {

struct fixture {
  explicit fixture(std::string text = {}, bool multiline = false,
                   basalt::size viewport = {400, 300})
      : f(viewport) {
    root = &f.tree.emplace_root<box>(axis::vertical);
    root->set_cross_alignment(alignment::start);
    field = &root->emplace_child<text_field>(std::move(text), multiline);
    field->set_accessible_name("Field");
    field->set_on_change([this](std::string const &v) { changes.push_back(v); });
    field->set_clipboard(
        {[this] { return board; }, [this](std::string v) { board = std::move(v); }});
    f.layout();
    field->request_focus();
  }

  ui f;
  box *root = nullptr;
  text_field *field = nullptr;
  std::vector<std::string> changes;
  std::string board;
};

auto click_event(point at, int count) -> pointer_event {
  return {.action = pointer_action::down,
          .position = at,
          .button = pointer_button::primary,
          .buttons = pointer_button::primary,
          .click_count = static_cast<std::uint8_t>(count)};
}

} // namespace

TEST_CASE("typing inserts at the caret and reports the change", "[text_field]") {
  fixture f("ac");
  f.field->set_selection(1, 1);
  f.f.type("b");
  REQUIRE(f.field->text() == "abc");
  REQUIRE(f.field->selection().focus == 2);
  REQUIRE(f.changes == std::vector<std::string>{"abc"});
}

TEST_CASE("set_text is silent and puts the caret at the end", "[text_field]") {
  fixture f;
  f.field->set_text("hello");
  REQUIRE(f.changes.empty());
  REQUIRE(f.field->selection().focus == 5);
}

TEST_CASE("backspace removes a whole grapheme", "[text_field]") {
  fixture f("e\xCC\x81x");
  f.f.tap(key::backspace);
  REQUIRE(f.field->text() == "e\xCC\x81");
  f.f.tap(key::backspace);
  REQUIRE(f.field->text().empty());
  f.f.tap(key::backspace);
  REQUIRE(f.changes.size() == 2);
}

TEST_CASE("delete removes forward and replaces a selection", "[text_field]") {
  fixture f("abcd");
  f.field->set_selection(1, 1);
  f.f.tap(key::delete_forward);
  REQUIRE(f.field->text() == "acd");
  f.field->set_selection(0, 2);
  f.f.tap(key::delete_forward);
  REQUIRE(f.field->text() == "d");
}

TEST_CASE("shift extends the selection and plain arrows collapse it", "[text_field]") {
  fixture f("hello");
  f.field->set_selection(0, 0);
  f.f.tap(key::right, modifier::shift);
  f.f.tap(key::right, modifier::shift);
  REQUIRE(f.field->selected_text() == "he");
  f.f.tap(key::right);
  REQUIRE_FALSE(f.field->has_selection());
  REQUIRE(f.field->selection().focus == 2);
  f.f.tap(key::left, modifier::shift);
  f.f.tap(key::left);
  REQUIRE(f.field->selection().focus == 1);
}

TEST_CASE("alt or control moves by word", "[text_field]") {
  fixture f("hello brave world");
  f.field->set_selection(0, 0);
  f.f.tap(key::right, modifier::alt);
  REQUIRE(f.field->selection().focus == 5);
  f.f.tap(key::right, modifier::alt);
  REQUIRE(f.field->selection().focus == 11);
  f.f.tap(key::left, modifier::control);
  REQUIRE(f.field->selection().focus == 6);
  f.f.tap(key::backspace, modifier::alt);
  REQUIRE(f.field->text() == "brave world");
}

TEST_CASE("home, end and select all", "[text_field]") {
  fixture f("hello");
  f.f.tap(key::home);
  REQUIRE(f.field->selection().focus == 0);
  f.f.tap(key::end, modifier::shift);
  REQUIRE(f.field->selected_text() == "hello");
  f.f.tap(key::home);
  f.f.tap(key::a, modifier::control);
  REQUIRE(f.field->selected_text() == "hello");
}

TEST_CASE("clipboard shortcuts copy, cut and paste", "[text_field]") {
  fixture f("hello world");
  f.field->set_selection(0, 5);
  f.f.tap(key::c, modifier::control);
  REQUIRE(f.board == "hello");
  f.f.tap(key::x, modifier::meta);
  REQUIRE(f.field->text() == " world");
  f.field->set_selection(6, 6);
  f.f.tap(key::v, modifier::control);
  REQUIRE(f.field->text() == " worldhello");
}

TEST_CASE("a single-line field flattens line breaks and submits on Enter", "[text_field]") {
  fixture f;
  std::string submitted;
  f.field->set_on_submit([&](std::string const &v) { submitted = v; });
  f.board = "a\r\nb\nc";
  f.f.tap(key::v, modifier::control);
  REQUIRE(f.field->text() == "a b c");
  REQUIRE(f.f.tap(key::enter));
  REQUIRE(submitted == "a b c");
  REQUIRE(f.field->text() == "a b c");
}

TEST_CASE("Enter in a multi-line field inserts a line break", "[text_field]") {
  fixture f("ab", true);
  f.field->set_selection(1, 1);
  f.f.tap(key::enter);
  REQUIRE(f.field->text() == "a\nb");
}

TEST_CASE("a read-only field allows selection and copy but no edits", "[text_field]") {
  fixture f("hello");
  f.field->set_read_only(true);
  REQUIRE_FALSE(f.f.type("x"));
  REQUIRE_FALSE(f.f.tap(key::backspace));
  f.f.tap(key::a, modifier::control);
  f.f.tap(key::c, modifier::control);
  REQUIRE(f.board == "hello");
  REQUIRE(f.field->text() == "hello");
  REQUIRE(f.changes.empty());
}

TEST_CASE("input method composition is shown, then committed", "[text_field]") {
  fixture f("ab");
  f.field->set_selection(1, 1);
  f.f.send(ime_event{.text = "\xE3\x81\x8B"});
  REQUIRE(f.field->preedit() == "\xE3\x81\x8B");
  REQUIRE(f.field->text() == "ab");
  f.f.type("\xE3\x81\x8B");
  REQUIRE(f.field->preedit().empty());
  REQUIRE(f.field->text() == "a\xE3\x81\x8B"
                             "b");

  f.f.send(ime_event{.text = "x"});
  f.f.send(ime_event{.phase = ime_phase::cancel});
  REQUIRE(f.field->preedit().empty());
}

TEST_CASE("losing focus drops the composition", "[text_field]") {
  fixture f("ab");
  f.f.send(ime_event{.text = "x"});
  f.f.tree.focus().clear_focus();
  REQUIRE(f.field->preedit().empty());
}

TEST_CASE("clicks place the caret and multi-clicks select", "[text_field]") {
  fixture f("hello world");
  rect const area = f.field->bounds();
  f.f.send(press({area.left() + 1, area.top() + 5}));
  f.f.send(release({area.left() + 1, area.top() + 5}));
  REQUIRE(f.field->selection().focus == 0);
  f.f.send(click_event({area.right() - 2, area.top() + 5}, 2));
  REQUIRE(f.field->selected_text() == "world");
  f.f.send(release({area.right() - 2, area.top() + 5}));
  f.f.send(click_event({area.right() - 2, area.top() + 5}, 3));
  REQUIRE(f.field->selected_text() == "hello world");
}

TEST_CASE("text fields expose text field semantics", "[text_field][accessibility]") {
  fixture f("hello");
  f.field->set_selection(1, 3);
  auto const snapshot = f.f.semantics();
  auto const *node = snapshot.find(f.field->id());
  REQUIRE(node->semantics.kind == a11y::role::text_field);
  REQUIRE(node->semantics.name == "Field");
  REQUIRE(node->semantics.value == "hello");
  REQUIRE(node->semantics.selection == a11y::text_selection{1, 3});
  REQUIRE(node->supports(a11y::action::set_value));
  REQUIRE(a11y::audit(snapshot).empty());

  a11y::action_request set{a11y::action::set_value};
  set.text = "bye";
  REQUIRE(a11y::perform_action(f.f.tree, f.field->id(), set));
  REQUIRE(f.field->text() == "bye");
  a11y::action_request select{a11y::action::set_text_selection};
  select.selection = {0, 2};
  REQUIRE(a11y::perform_action(f.f.tree, f.field->id(), select));
  REQUIRE(f.field->selected_text() == "by");

  f.field->set_read_only(true);
  REQUIRE_FALSE(f.f.semantics().find(f.field->id())->supports(a11y::action::set_value));
}

TEST_CASE("shaped text gives the caret and hit testing real positions", "[text_field]") {
  auto &fonts = med::test::system_fonts();
  if (!med::test::has_coverage(fonts, U'a')) {
    SKIP("no font covers Latin letters");
  }
  text::text_shaper shaper{fonts};
  fixture f("hello");
  f.f.tree.set_text_shaper(&shaper);
  f.f.layout();

  f.field->set_selection(0, 0);
  float const start = f.field->caret_rect().left();
  f.field->set_selection(5, 5);
  float const end = f.field->caret_rect().left();
  REQUIRE(end > start);
  f.field->set_selection(3, 3);
  point const at3{f.field->caret_rect().left() + 1, f.field->caret_rect().top() + 2};
  REQUIRE(f.field->offset_at(at3) == 3);

  rect const area = f.field->bounds();
  f.f.send(press({area.left() + at3.x, area.top() + at3.y}));
  f.f.send(release({area.left() + at3.x, area.top() + at3.y}));
  REQUIRE(f.field->selection().focus == 3);
  f.f.tree.set_text_shaper(nullptr);
}

TEST_CASE("up and down keep the column in a multi-line field", "[text_field]") {
  auto &fonts = med::test::system_fonts();
  if (!med::test::has_coverage(fonts, U'a')) {
    SKIP("no font covers Latin letters");
  }
  text::text_shaper shaper{fonts};
  fixture f("aa\naa\naa", true);
  f.f.tree.set_text_shaper(&shaper);
  f.f.layout();
  f.field->set_selection(1, 1);
  f.f.tap(key::down);
  REQUIRE(f.field->selection().focus == 4);
  f.f.tap(key::down);
  REQUIRE(f.field->selection().focus == 7);
  f.f.tap(key::down);
  REQUIRE(f.field->selection().focus == 8);
  f.f.tap(key::up);
  f.f.tap(key::up);
  f.f.tap(key::up);
  REQUIRE(f.field->selection().focus == 0);
  f.f.tree.set_text_shaper(nullptr);
}

TEST_CASE("a multi-line field grows as its text wraps", "[text_field]") {
  auto &fonts = med::test::system_fonts();
  if (!med::test::has_coverage(fonts, U'a')) {
    SKIP("no font covers Latin letters");
  }
  text::text_shaper shaper{fonts};
  fixture f("one two three four five six seven eight nine ten", true, {120, 600});
  f.f.tree.set_text_shaper(&shaper);
  f.f.layout();
  float const wrapped = f.field->bounds().height();
  f.field->set_text("one");
  f.f.layout();
  REQUIRE(wrapped > f.field->bounds().height());
  f.f.tree.set_text_shaper(nullptr);
}
