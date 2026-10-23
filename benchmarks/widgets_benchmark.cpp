// Benchmarks for the caching paths of text_line, select and text_field. They use only public API,
// so the same file builds against an older revision for a before and after comparison.
//
// Run them with `just bench`. Catch2 warms up, estimates the clock cost, takes many samples and
// reports a bootstrap confidence interval. Compare two runs by their means and intervals; a change
// is real only when the intervals do not overlap.
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include "basalt/paint.h"
#include "basalt/text/paragraph.h"
#include "basalt/text/text_layout.h"
#include "basalt/widget_painter.h"
#include "basalt/widgets/box.h"
#include "basalt/widgets/collection_view.h"
#include "basalt/widgets/image_tile.h"
#include "basalt/widgets/label.h"
#include "basalt/widgets/select.h"
#include "basalt/widgets/text_field.h"
#include "basalt/widgets/text_line.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

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

// The same document and edit as the text_field benchmark, on text_layout, which lays out only the
// blocks an edit touches. It gives the cost to compare when deciding whether text_field should use
// it. `block_count` lines of `words` words each; the field above has 40 blocks of 40 words.
namespace {

auto layout_edit_benchmarks(std::size_t blocks, std::size_t words) -> void {
  scene s({400, 4000});
  std::string text;
  for (std::size_t line = 0; line < blocks; ++line) {
    text += sentence(words) + '\n';
  }
  basalt::theme const theme;
  text::text_layout_options options;
  options.paragraph.shape.style = to_layout(text_style_for(theme, text_role::body), {});
  options.paragraph.wrap = true;
  text::text_layout document{s.shaper, text, options};
  document.set_width(368);
  (void)document.layout_range(0, 1e9f);

  BENCHMARK("type a character and delete it, then caret and visible blocks") {
    document.replace(100, 100, "x");
    document.replace(100, 101, "");
    (void)document.layout_range(0, 4000);
    return document.caret_rect(100).left();
  };
}

} // namespace

TEST_CASE("text_layout", "[benchmark]") {
  require_optimized_build();
  SECTION("40 blocks of 40 words") { layout_edit_benchmarks(40, 40); }
  SECTION("1 block of 1600 words") { layout_edit_benchmarks(1, 1600); }
}

// Phase 6 measurements: shaping cost under wrapping and accessibility snapshot cost on a deep tree.
TEST_CASE("text_shaper", "[benchmark]") {
  require_optimized_build();
  scene s;
  basalt::theme const theme;
  text::paragraph_options options;
  options.shape.style = to_layout(text_style_for(theme, text_role::body), {});
  options.wrap = true;
  options.max_width = 300;
  auto const long_text = sentence(400);
  auto const line_text = sentence(8);

  BENCHMARK("shape one line") { return s.shaper.shape(line_text, options.shape).width; };

  auto spanned = options.shape;
  spanned.spans = {{.begin = 4, .end = 9, .bold = true}, {.begin = 14, .end = 19, .italic = true}};
  BENCHMARK("shape one line with two spans") { return s.shaper.shape(line_text, spanned).width; };

  BENCHMARK("lay out a wrapped paragraph of 400 words") {
    text::paragraph const p{s.shaper, long_text, options};
    return p.lines().size();
  };
}

TEST_CASE("accessibility", "[benchmark]") {
  require_optimized_build();
  scene s({600, 8000});
  // About 3000 labeled nodes, nested ten levels deep.
  auto *parent = s.root;
  for (int depth = 0; depth < 10; ++depth) {
    parent = &parent->emplace_child<box>(axis::vertical);
    for (int i = 0; i < 300; ++i) {
      parent->emplace_child<label>("item " + std::to_string(depth) + "." + std::to_string(i));
    }
  }
  s.warm();

  BENCHMARK("build the accessibility tree") { return s.f.semantics().size(); };
}

// A grid of 50,000 picture tiles in a window of about 20 visible rows. The tiles share one image,
// so the measurements cover the grid's layout, cell reuse and painting, not image decoding.
TEST_CASE("collection_view", "[benchmark]") {
  require_optimized_build();
  ui f({1200, 800});
  auto const shared = raster_image::create(8, 8, std::vector<std::uint8_t>(8 * 8 * 4, 128));
  auto &grid = f.tree.emplace_root<collection_view>("Photos");
  grid.set_cell_size(fixed(100), fixed(100));
  grid.set_gap(fixed(8));
  grid.set_cell_factory([&](std::size_t index, std::unique_ptr<widget> recycled) {
    std::unique_ptr<image_tile> tile;
    if (recycled != nullptr) {
      tile.reset(static_cast<image_tile *>(recycled.release()));
    } else {
      tile = std::make_unique<image_tile>();
    }
    tile->set_image(shared);
    tile->set_selected(index % 7 == 0);
    return tile;
  });
  grid.set_item_count(50'000);
  f.layout();
  (void)f.commands();
  REQUIRE(grid.realized_items().size() > 100);
  REQUIRE(f.commands().size() > 100);

  BENCHMARK("paint, nothing changed") { return f.commands().size(); };

  float offset = 0;
  BENCHMARK("scroll by 60 pixels, layout and paint") {
    offset += 60;
    grid.set_scroll_offset(offset);
    f.layout();
    return f.commands().size();
  };

  bool far = false;
  BENCHMARK("jump across the whole grid, layout and paint") {
    far = !far;
    grid.set_scroll_offset(far ? 200'000.0F : 0.0F);
    f.layout();
    return f.commands().size();
  };

  BENCHMARK("change the count, layout and paint") {
    grid.set_item_count(far ? 50'000 : 49'999);
    far = !far;
    f.layout();
    return f.commands().size();
  };
}
