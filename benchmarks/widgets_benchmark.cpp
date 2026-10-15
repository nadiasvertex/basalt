// Benchmarks for the caching paths of text_line, select and text_field. They use only public API,
// so the same file builds against an older revision for a before and after comparison.
//
// Run them with `just bench`. Catch2 warms up, estimates the clock cost, takes many samples and
// reports a bootstrap confidence interval. Compare two runs by their means and intervals; a change
// is real only when the intervals do not overlap.
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include "basalt/paint.h"
#include "basalt/widget_painter.h"
#include "basalt/widgets/box.h"
#include "basalt/widgets/label.h"
#include "basalt/widgets/select.h"
#include "basalt/widgets/text_field.h"
#include "basalt/widgets/text_line.h"

#include <cstddef>
#include <string>

#include "text/font_support.h"
#include "ui_fixture.h"

using namespace basalt;
using namespace basalt::testing;

namespace {

// Debug builds and sanitizers measure the instrumentation, so they are refused.
auto require_optimized_build() -> void {
#ifndef NDEBUG
  FAIL("Benchmarks need an optimized build. Use `just bench`.");
#endif
}

auto sentence(std::size_t words) -> std::string {
  constexpr std::string_view pool[] = {"the",  "quick", "brown",  "fox",   "jumps",
                                       "over", "a",     "lazy",   "dog",   "and",
                                       "runs", "into",  "forest", "where", "owls"};
  std::string out;
  for (std::size_t i = 0; i < words; ++i) {
    out += pool[i % std::size(pool)];
    out += i + 1 < words ? ' ' : '\0';
  }
  if (!out.empty() && out.back() == '\0') {
    out.pop_back();
  }
  return out;
}

// A tree with the bundled fonts, laid out and painted once so that every cache is warm.
struct scene {
  explicit scene(basalt::size viewport = {600, 800}) : f(viewport), shaper(fonts()) {
    f.tree.set_text_shaper(&shaper);
    root = &f.tree.emplace_root<box>(axis::vertical);
    root->set_cross_alignment(alignment::start);
  }
  ~scene() { f.tree.set_text_shaper(nullptr); }

  static auto fonts() -> text::font_manager & { return basalt::test::bundled_fonts(); }

  auto warm() -> void {
    f.layout();
    (void)f.commands();
  }

  ui f;
  text::text_shaper shaper;
  box *root = nullptr;
};

} // namespace

TEST_CASE("text_line", "[benchmark]") {
  require_optimized_build();
  scene s;
  auto &owner = s.root->emplace_child<label>("owner");
  s.warm();

  text_line line{sentence(20)};
  (void)line.measure(owner);

  BENCHMARK("measure, nothing changed") { return line.measure(owner); };

  text_line const other{sentence(20) + " again"};
  std::string const first = sentence(20);
  std::string const second = other.text();
  bool flip = false;
  BENCHMARK("set_text to a new string, then measure") {
    flip = !flip;
    line.set_text(flip ? second : first);
    return line.measure(owner);
  };

  BENCHMARK("set_text to the same string, then measure") {
    line.set_text(first);
    return line.measure(owner);
  };
}

TEST_CASE("select", "[benchmark]") {
  require_optimized_build();
  scene s;
  auto &field = s.root->emplace_child<select>("Fruit");
  field.set_mode(select_mode::multiple);
  field.set_placeholder("Pick some fruit");
  for (std::size_t i = 0; i < 40; ++i) {
    field.add_option("Option number " + std::to_string(i));
  }
  field.choose(3);
  field.choose(17);
  field.choose(31);
  s.warm();

  BENCHMARK("paint, nothing changed") { return s.f.commands().size(); };

  std::size_t next = 0;
  BENCHMARK("toggle one option, layout and paint") {
    field.choose(next % 40, next % 2 == 0);
    ++next;
    s.f.layout();
    return s.f.commands().size();
  };

  BENCHMARK("read accessibility value") { return s.f.semantics().size(); };
}

TEST_CASE("text_field", "[benchmark]") {
  require_optimized_build();
  scene s({400, 4000});
  std::string text;
  for (int line = 0; line < 40; ++line) {
    text += sentence(40) + '\n';
  }
  auto &field = s.root->emplace_child<text_field>(text, true);
  field.set_accessible_name("Notes");
  s.warm();
  field.request_focus();
  field.set_selection(100, 100);
  s.f.layout();

  BENCHMARK("caret rect, nothing changed") { return field.caret_rect().left(); };
  BENCHMARK("paint, nothing changed") { return s.f.commands().size(); };

  BENCHMARK("type a character and delete it, with layout and caret") {
    s.f.type("x");
    s.f.tap(key::backspace);
    return field.caret_rect().left();
  };
}
