#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "basalt/text/text_layout.h"

#include <string>

#include "font_support.h"

using namespace basalt;
using namespace basalt::text;
using Catch::Matchers::WithinAbs;

namespace {

auto options() -> text_layout_options {
  auto result = text_layout_options{};
  result.paragraph.shape.style = text_style{"Noto Sans", 20};
  return result;
}

auto require_latin() -> font_manager & { return basalt::test::bundled_fonts(); }

auto numbered_lines(int count) -> std::string {
  auto text = std::string{};
  for (auto i = 0; i < count; ++i) {
    text += "line " + std::to_string(i) + (i + 1 < count ? "\n" : "");
  }
  return text;
}

auto lines_to_blocks(std::string_view text, std::size_t index) -> std::string {
  auto begin = std::size_t{0};
  for (auto i = std::size_t{0}; i < index; ++i) {
    begin = text.find('\n', begin) + 1;
  }
  return std::string{text.substr(begin, text.find('\n', begin) - begin)};
}

} // namespace

TEST_CASE("text splits into blocks at line breaks", "[text_layout]") {
  auto &fonts = require_latin();
  auto const shaper = text_shaper{fonts};
  auto const layout = text_layout{shaper, "a\nb\r\nc", options()};
  REQUIRE(layout.block_count() == 3);
  CHECK(layout.block_begin(0) == 0);
  CHECK(layout.block_end(0) == 1);
  CHECK(layout.block_begin(1) == 2);
  CHECK(layout.block_end(1) == 3);
  CHECK(layout.block_begin(2) == 5);
  CHECK(layout.block_end(2) == 6);
  CHECK(layout.block_at_offset(0) == 0);
  CHECK(layout.block_at_offset(3) == 1);
  CHECK(layout.block_at_offset(4) == 1);
  CHECK(layout.block_at_offset(6) == 2);
}

TEST_CASE("empty text and trailing breaks make empty blocks", "[text_layout]") {
  auto &fonts = require_latin();
  auto const shaper = text_shaper{fonts};
  CHECK(text_layout{shaper, "", options()}.block_count() == 1);
  auto const layout = text_layout{shaper, "a\n", options()};
  REQUIRE(layout.block_count() == 2);
  CHECK(layout.block_begin(1) == 2);
  CHECK(layout.block_end(1) == 2);
}

TEST_CASE("only blocks near the viewport are shaped", "[text_layout]") {
  auto &fonts = require_latin();
  auto const shaper = text_shaper{fonts};
  auto layout = text_layout{shaper, numbered_lines(2000), options()};
  auto const range = layout.layout_range(0, 100);
  CHECK(range.first == 0);
  CHECK(range.last > 1);
  CHECK(range.last < 20);
  for (auto i = range.first; i < range.last; ++i) {
    CHECK(layout.is_measured(i));
  }
  CHECK_FALSE(layout.is_measured(1999));
  CHECK(layout.cached_paragraph_count() < 20);
  CHECK(layout.total_height() > 2000 * 10);
}

TEST_CASE("block tops stack with spacing", "[text_layout]") {
  auto &fonts = require_latin();
  auto const shaper = text_shaper{fonts};
  auto opts = options();
  opts.block_spacing = 7;
  auto layout = text_layout{shaper, numbered_lines(50), opts};
  layout.layout_range(0, 400);
  CHECK(layout.block_top(0) == 0);
  auto const range = layout.layout_range(0, 400);
  for (auto i = std::size_t{0}; i + 1 < range.last; ++i) {
    CHECK_THAT(layout.block_top(i + 1),
               WithinAbs(layout.block_top(i) + layout.block_height(i) + 7, 0.001));
  }
  // The measured range covers the viewport.
  CHECK(layout.block_top(range.last - 1) < 400);
  CHECK(layout.block_top(range.last - 1) + layout.block_height(range.last - 1) + 7 >= 400);
}

TEST_CASE("layout_range finds blocks at a scrolled position", "[text_layout]") {
  auto &fonts = require_latin();
  auto const shaper = text_shaper{fonts};
  auto layout = text_layout{shaper, numbered_lines(1000), options()};
  auto const range = layout.layout_range(10000, 10300);
  CHECK(range.first > 100);
  CHECK(layout.block_top(range.first) <= 10000);
  CHECK(layout.block_top(range.first) + layout.block_height(range.first) > 10000);
  CHECK(layout.block_top(range.last - 1) < 10300);
  CHECK(layout.block_at_y(10000) == range.first);
  for (auto i = range.first; i < range.last; ++i) {
    CHECK(layout.is_measured(i));
  }
  // Scrolling past the end clamps to the last block.
  auto const end = layout.layout_range(1e9F, 1e9F + 100);
  CHECK(end.last == layout.block_count());
}

TEST_CASE("estimates move toward measurements for wrapped text", "[text_layout]") {
  auto &fonts = require_latin();
  auto const shaper = text_shaper{fonts};
  auto text = std::string{};
  for (auto i = 0; i < 100; ++i) {
    text += "the quick brown fox jumps over the lazy dog and keeps running along\n";
  }
  auto layout = text_layout{shaper, text, options()};
  layout.set_width(150);
  auto const estimate = layout.block_height(50);
  CHECK(estimate > 20 * 2);
  CHECK_FALSE(layout.is_measured(50));
  REQUIRE(layout.paragraph_of(50) != nullptr);
  CHECK(layout.is_measured(50));
  CHECK(layout.block_height(50) > 20 * 2);
  CHECK(layout.block_height(50) == layout.paragraph_of(50)->extent().height);
}

TEST_CASE("narrower widths wrap blocks into more lines", "[text_layout]") {
  auto &fonts = require_latin();
  auto const shaper = text_shaper{fonts};
  auto const text = std::string{"aaaa bbbb cccc dddd\nshort"};
  auto layout = text_layout{shaper, text, options()};
  layout.set_width(1000);
  auto const wide = layout.paragraph_of(0)->lines().size();
  CHECK(layout.is_measured(0));
  layout.set_width(100);
  CHECK_FALSE(layout.is_measured(0));
  CHECK(layout.paragraph_of(0)->lines().size() > wide);
  // Spaces at a soft wrap may hang past the width; the rest of the line must fit.
  auto const &first = layout.paragraph_of(0)->lines().front();
  auto visible = std::string_view{text}.substr(first.begin, first.end - first.begin);
  visible = visible.substr(0, visible.find_last_not_of(' ') + 1);
  CHECK(shaper.shape(visible, options().paragraph.shape).width <= 100.5F);
}

TEST_CASE("hit testing maps points to document offsets", "[text_layout]") {
  auto &fonts = require_latin();
  auto const shaper = text_shaper{fonts};
  auto layout = text_layout{shaper, "first\nsecond\nthird", options()};
  layout.layout_range(0, 1000);
  auto const second = layout.block_top(1) + layout.block_height(1) / 2;
  auto const hit = layout.hit_test({0, second});
  CHECK(hit.offset == 6);
  CHECK(layout.hit_test({2000, second}).offset == 12);
  CHECK(layout.hit_test({0, layout.block_top(2) + 3}).offset == 13);
  CHECK(layout.hit_test({0, -100}).offset == 0);
  CHECK(layout.hit_test({2000, 1e6F}).offset == 18);
}

TEST_CASE("caret rectangles lie in their block", "[text_layout]") {
  auto &fonts = require_latin();
  auto const shaper = text_shaper{fonts};
  auto layout = text_layout{shaper, "first\nsecond\nthird", options()};
  auto const caret = layout.caret_rect(9);
  CHECK(caret.top() == layout.block_top(1));
  auto const in_block = layout.paragraph_of(1)->caret_rect(3);
  CHECK(caret.left() == in_block.left());
  CHECK(caret.height() == in_block.height());
  // An offset on the line break clamps to the end of its block.
  CHECK(layout.caret_rect(5).top() == 0);
  CHECK(layout.caret_rect(18).top() == layout.block_top(2));
}

TEST_CASE("least recently used paragraphs are evicted but keep their height", "[text_layout]") {
  auto &fonts = require_latin();
  auto const shaper = text_shaper{fonts};
  auto opts = options();
  opts.max_cached_paragraphs = 8;
  auto layout = text_layout{shaper, numbered_lines(100), opts};
  for (auto i = std::size_t{0}; i < 100; ++i) {
    REQUIRE(layout.paragraph_of(i) != nullptr);
    CHECK(layout.cached_paragraph_count() <= 8);
  }
  CHECK(layout.is_measured(0));
  auto const height = layout.block_height(0);
  CHECK(height > 0);
  CHECK(layout.paragraph_of(0)->extent().height == height);
}

TEST_CASE("replace edits the text and keeps measurements of other blocks", "[text_layout]") {
  auto &fonts = require_latin();
  auto const shaper = text_shaper{fonts};
  auto layout = text_layout{shaper, numbered_lines(100), options()};
  layout.layout_range(0, 5000);
  auto const text = std::string{layout.text()};
  REQUIRE(layout.is_measured(10));
  auto const height_10 = layout.block_height(10);

  // Inside one block.
  layout.replace(5, 6, "XYZ");
  CHECK(layout.block_count() == 100);
  CHECK(layout.text().substr(0, 8) == "line XYZ");
  CHECK(layout.block_end(0) == 8);
  CHECK(layout.block_begin(1) == 9);
  CHECK_FALSE(layout.is_measured(0));
  CHECK(layout.is_measured(10));
  CHECK(layout.block_height(10) == height_10);
  CHECK(layout.text().size() == text.size() + 2);
  CHECK(layout.block_begin(10) == layout.text().find("line 10"));

  // Splitting a block.
  layout.replace(4, 4, "\n\n");
  CHECK(layout.block_count() == 102);
  CHECK(layout.block_end(0) == 4);
  CHECK(layout.block_begin(1) == 5);
  CHECK(layout.block_end(1) == 5);
  CHECK(layout.block_begin(2) == 6);
  CHECK(layout.is_measured(12));
  CHECK(layout.block_begin(12) == layout.text().find("line 10"));

  // Joining blocks by removing a break.
  layout.replace(4, 6, "");
  CHECK(layout.block_count() == 100);
  CHECK(layout.text() == [&] {
    auto expected = text;
    expected.replace(5, 1, "XYZ");
    return expected;
  }());
}

TEST_CASE("replace handles the ends and the whole text", "[text_layout]") {
  auto &fonts = require_latin();
  auto const shaper = text_shaper{fonts};
  auto layout = text_layout{shaper, "abc\ndef", options()};
  layout.replace(7, 7, "\nghi");
  CHECK(layout.text() == "abc\ndef\nghi");
  CHECK(layout.block_count() == 3);
  CHECK(layout.block_begin(2) == 8);
  layout.replace(0, 0, "x\n");
  CHECK(layout.block_count() == 4);
  CHECK(layout.block_end(0) == 1);
  layout.replace(0, layout.text().size(), "");
  CHECK(layout.text().empty());
  CHECK(layout.block_count() == 1);
  layout.replace(0, 0, "one\r\ntwo");
  CHECK(layout.block_count() == 2);
  CHECK(layout.block_end(0) == 3);
  CHECK(layout.block_begin(1) == 5);
  // Out-of-range edits clamp.
  layout.replace(100, 200, "!");
  CHECK(layout.text().ends_with("two!"));
}

TEST_CASE("editing keeps lookups consistent with a fresh layout", "[text_layout]") {
  auto &fonts = require_latin();
  auto const shaper = text_shaper{fonts};
  auto edited = text_layout{shaper, numbered_lines(30), options()};
  edited.layout_range(0, 2000);
  edited.replace(10, 40, "changed\ntext here\nand more");
  auto fresh = text_layout{shaper, std::string{edited.text()}, options()};
  edited.layout_range(0, 2000);
  fresh.layout_range(0, 2000);
  REQUIRE(edited.block_count() == fresh.block_count());
  for (auto i = std::size_t{0}; i < edited.block_count(); ++i) {
    CHECK(edited.block_begin(i) == fresh.block_begin(i));
    CHECK(edited.block_end(i) == fresh.block_end(i));
    CHECK_THAT(edited.block_top(i), WithinAbs(fresh.block_top(i), 0.01));
  }
}

TEST_CASE("selection rectangles span blocks and honor the clip", "[text_layout]") {
  auto &fonts = require_latin();
  auto const shaper = text_shaper{fonts};
  auto layout = text_layout{shaper, "first\nsecond\nthird\nfourth", options()};
  layout.layout_range(0, 1000);
  auto const all = layout.selection_rects(2, 20);
  // Four blocks, each with text, and a break marker merged into the first
  // three.
  CHECK(all.size() == 4);
  CHECK(all[0].top() == layout.block_top(0));
  CHECK(all[3].top() == layout.block_top(3));
  auto const clipped = layout.selection_rects(0, 24, layout.block_top(2), layout.block_top(2) + 5);
  REQUIRE(clipped.size() == 1);
  CHECK(clipped[0].top() == layout.block_top(2));
  CHECK(layout.selection_rects(3, 3).empty());
  // An empty block in the middle of a selection still shows.
  auto gap = text_layout{shaper, "a\n\nb", options()};
  auto const rects = gap.selection_rects(0, 4);
  REQUIRE(rects.size() == 3);
  CHECK(rects[1].top() == gap.block_top(1));
  CHECK(rects[1].width() > 0);
}

TEST_CASE("selecting a huge range does not measure hidden blocks", "[text_layout]") {
  auto &fonts = require_latin();
  auto const shaper = text_shaper{fonts};
  auto layout = text_layout{shaper, numbered_lines(5000), options()};
  layout.layout_range(0, 200);
  auto const rects = layout.selection_rects(0, layout.text().size(), 0, 200);
  CHECK_FALSE(rects.empty());
  CHECK(layout.cached_paragraph_count() < 30);
  CHECK_FALSE(layout.is_measured(4000));
}

TEST_CASE("per-block options style individual blocks", "[text_layout]") {
  auto &fonts = require_latin();
  auto const shaper = text_shaper{fonts};
  auto opts = options();
  opts.block_options = [base = opts.paragraph](std::size_t block, std::string_view) {
    auto result = base;
    if (block == 0) {
      result.shape.style.font_size = 40;
    }
    return result;
  };
  auto layout = text_layout{shaper, "Title\nBody", opts};
  CHECK(layout.block_height(0) > 0);
  layout.layout_range(0, 1000);
  CHECK(layout.block_height(0) > layout.block_height(1) * 1.5F);
  CHECK(lines_to_blocks(layout.text(), 1) == "Body");
}

TEST_CASE("a block footer adds height below the text and moves later blocks", "[text_layout]") {
  auto &fonts = require_latin();
  auto const shaper = text_shaper{fonts};
  auto plain = text_layout{shaper, "a\nb\nc", options()};
  plain.set_width(400);
  plain.layout_range(0, plain.total_height());

  auto with_footer = options();
  with_footer.block_footer = [](std::size_t block) { return block == 1 ? 17.0F : 0.0F; };
  auto layout = text_layout{shaper, "a\nb\nc", with_footer};
  layout.set_width(400);
  layout.layout_range(0, layout.total_height());

  CHECK_THAT(layout.block_height(0), WithinAbs(plain.block_height(0), 0.01));
  CHECK_THAT(layout.block_height(1), WithinAbs(plain.block_height(1) + 17.0F, 0.01));
  CHECK_THAT(layout.block_top(2), WithinAbs(plain.block_top(2) + 17.0F, 0.01));
  CHECK_THAT(layout.total_height(), WithinAbs(plain.total_height() + 17.0F, 0.01));
  // The text of the block keeps its place; a point in the footer still hits that block.
  CHECK(layout.block_at_y(layout.block_top(1) + layout.block_height(1) - 1) == 1);
  CHECK(layout.caret_rect(2).top() == plain.caret_rect(2).top());
}

TEST_CASE("an unmeasured block's estimate includes its footer", "[text_layout]") {
  auto &fonts = require_latin();
  auto const shaper = text_shaper{fonts};
  auto with_footer = options();
  with_footer.block_footer = [](std::size_t) { return 10.0F; };
  auto layout = text_layout{shaper, "a\nb", with_footer};
  layout.set_width(400);
  auto plain = text_layout{shaper, "a\nb", options()};
  plain.set_width(400);
  CHECK_THAT(layout.total_height(), WithinAbs(plain.total_height() + 20.0F, 0.01));
}

TEST_CASE("one block can be laid out again with new options", "[text_layout]") {
  auto &fonts = require_latin();
  auto const shaper = text_shaper{fonts};
  auto padding = 0.0F;
  auto opts = options();
  opts.block_options = [&](std::size_t block, std::string_view) {
    auto result = opts.paragraph;
    result.line_padding = block == 1 ? padding : 0.0F;
    return result;
  };
  auto layout = text_layout{shaper, numbered_lines(3), opts};
  layout.set_width(400);
  layout.layout_range(0, layout.total_height());
  auto const before = layout.block_height(1);
  auto const third_top = layout.block_top(2);

  padding = 9.0F;
  // Nothing changes until the block is invalidated.
  CHECK_THAT(layout.block_height(1), WithinAbs(before, 0.01));
  layout.invalidate_block(1);
  CHECK(layout.is_measured(0));
  CHECK(layout.is_measured(2));
  layout.layout_range(0, layout.total_height());
  CHECK_THAT(layout.block_height(1), WithinAbs(before + 9.0F, 0.01));
  CHECK_THAT(layout.block_top(2), WithinAbs(third_top + 9.0F, 0.01));
}

TEST_CASE("returning to a recent width finds its paragraphs in place", "[text_layout]") {
  auto &fonts = require_latin();
  auto const shaper = text_shaper{fonts};
  auto layout = text_layout{shaper, numbered_lines(40), options()};
  layout.set_width(400);
  layout.layout_range(0, layout.total_height());
  auto const shaped = layout.cached_paragraph_count();
  auto const height = layout.total_height();
  REQUIRE(shaped > 0);

  auto const first = layout.paragraph_of(3);
  layout.set_width(250);
  CHECK(layout.cached_paragraph_count() == 0);
  CHECK(layout.kept_width_count() == 1);
  layout.layout_range(0, layout.total_height());

  // The same paragraph objects come back, so nothing was shaped again.
  layout.set_width(400);
  CHECK(layout.cached_paragraph_count() == shaped);
  CHECK(layout.paragraph_of(3) == first);
  CHECK_THAT(layout.total_height(), WithinAbs(height, 0.01));
}

TEST_CASE("only a few recent widths are kept", "[text_layout]") {
  auto &fonts = require_latin();
  auto const shaper = text_shaper{fonts};
  auto layout = text_layout{shaper, numbered_lines(10), options()};
  for (auto const width : {300.0F, 310.0F, 320.0F, 330.0F, 340.0F}) {
    layout.set_width(width);
    layout.layout_range(0, layout.total_height());
  }
  CHECK(layout.kept_width_count() == 2);
}

TEST_CASE("edits and option changes drop the paragraphs kept for other widths", "[text_layout]") {
  auto &fonts = require_latin();
  auto const shaper = text_shaper{fonts};
  auto layout = text_layout{shaper, numbered_lines(10), options()};
  layout.set_width(400);
  layout.layout_range(0, layout.total_height());
  layout.set_width(250);
  layout.layout_range(0, layout.total_height());
  REQUIRE(layout.kept_width_count() == 1);

  layout.replace(0, 6, "longer text here");
  CHECK(layout.kept_width_count() == 0);
  layout.set_width(400);
  layout.layout_range(0, layout.total_height());
  auto const edited = layout.paragraph_of(0);
  CHECK(edited->text_size() == std::string_view{"longer text here"}.size());

  layout.set_width(250);
  REQUIRE(layout.kept_width_count() == 1);
  layout.set_options(options());
  CHECK(layout.kept_width_count() == 0);
}
