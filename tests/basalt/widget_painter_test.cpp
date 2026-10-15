#include <catch2/catch_test_macros.hpp>

#include "basalt/render_context.h"
#include "basalt/widget_painter.h"

#include <memory>
#include <vector>

#include "ui_fixture.h"

using namespace basalt;
using namespace basalt::testing;

namespace {

// Draws a rectangle of its size, and a marker over its children.
struct drawn : widget {
  explicit drawn(rect area = {}, rgba body = {1, 1, 1, 255}, rgba over = {2, 2, 2, 255})
      : body(body), over(over) {
    set_bounds(area);
  }
  rgba body;
  rgba over;
  bool overlay = false;
  mutable int paints = 0;

protected:
  auto on_paint(render_context &context) const -> void override {
    ++paints;
    context.fill_rect({{}, bounds().extent}, body);
  }
  auto on_paint_overlay(render_context &context) const -> void override {
    if (overlay) {
      context.fill_rect({{}, {1, 1}}, over);
    }
  }
};

auto fills(paint_commands const &commands) -> std::vector<rgba> {
  std::vector<rgba> colors;
  for (auto const &c : commands) {
    if (auto const *f = std::get_if<fill_rect_command>(&c)) {
      colors.push_back(f->color);
    }
  }
  return colors;
}

} // namespace

TEST_CASE("widgets paint back to front with overlays after their children", "[paint_tree]") {
  ui f;
  auto &root = f.tree.emplace_root<drawn>(rect{}, rgba{10, 0, 0, 255}, rgba{11, 0, 0, 255});
  root.overlay = true;
  root.add_child(std::make_unique<drawn>(rect{{5, 5}, {20, 20}}, rgba{20, 0, 0, 255}));
  root.add_child(std::make_unique<drawn>(rect{{30, 5}, {20, 20}}, rgba{30, 0, 0, 255}));
  f.layout();

  auto const colors = fills(f.commands());
  REQUIRE(colors ==
          std::vector<rgba>{{10, 0, 0, 255}, {20, 0, 0, 255}, {30, 0, 0, 255}, {11, 0, 0, 255}});
}

TEST_CASE("each widget draws in its own clipped coordinates", "[paint_tree]") {
  ui f;
  auto &root = f.tree.emplace_root<drawn>();
  root.add_child(std::make_unique<drawn>(rect{{5, 6}, {20, 30}}));
  f.layout();

  auto const commands = f.commands();
  std::vector<push_transform_command> moves;
  std::vector<push_clip_command> clips;
  for (auto const &c : commands) {
    if (auto const *m = std::get_if<push_transform_command>(&c)) {
      moves.push_back(*m);
    }
    if (auto const *k = std::get_if<push_clip_command>(&c)) {
      clips.push_back(*k);
    }
  }
  REQUIRE(moves.size() == 2);
  REQUIRE(moves[1].transform == affine_transform::translation(5, 6));
  REQUIRE(clips[1].bounds == rect{{0, 0}, {20, 30}});
  REQUIRE(count_commands<pop_clip_command>(commands) == 2);
  REQUIRE(count_commands<pop_transform_command>(commands) == 2);
}

TEST_CASE("hidden widgets and subtrees outside the damage are skipped", "[paint_tree]") {
  ui f;
  auto &root = f.tree.emplace_root<drawn>();
  auto &hidden = root.add_child(std::make_unique<drawn>(rect{{0, 0}, {10, 10}}));
  auto &left = root.add_child(std::make_unique<drawn>(rect{{0, 50}, {10, 10}}));
  auto &right = root.add_child(std::make_unique<drawn>(rect{{300, 50}, {10, 10}}));
  hidden.set_visible(false);
  f.layout();

  paint_commands out;
  paint_tree(f.tree, out, rect{{0, 0}, {100, 100}});
  REQUIRE(hidden.paints == 0);
  REQUIRE(left.paints == 1);
  REQUIRE(right.paints == 0);
}

TEST_CASE("a disabled subtree is drawn through one dimmed layer", "[paint_tree]") {
  ui f;
  auto &root = f.tree.emplace_root<drawn>();
  auto &panel = root.add_child(std::make_unique<drawn>(rect{{0, 0}, {50, 50}}));
  panel.add_child(std::make_unique<drawn>(rect{{0, 0}, {10, 10}}));
  panel.set_enabled(false);
  f.layout();

  auto const commands = f.commands();
  REQUIRE(count_commands<begin_layer_command>(commands) == 1);
  REQUIRE(count_commands<end_layer_command>(commands) == 1);
  for (auto const &c : commands) {
    if (auto const *layer = std::get_if<begin_layer_command>(&c)) {
      REQUIRE(layer->opacity ==
              static_cast<std::uint8_t>(f.tree.theme().disabled_opacity * 255 + 0.5));
    }
  }
}

TEST_CASE("the focus ring is drawn last when focus is visible", "[paint_tree]") {
  ui f;
  auto &root = f.tree.emplace_root<drawn>();
  auto &field = root.add_child(std::make_unique<drawn>(rect{{20, 20}, {50, 20}}));
  field.set_focus_policy(focus_policy::tab);
  f.layout();

  REQUIRE(count_commands<stroke_rounded_rect_command>(f.commands()) == 0);
  f.tap(key::tab);
  REQUIRE(field.has_focus());
  auto const commands = f.commands();
  REQUIRE(count_commands<stroke_rounded_rect_command>(commands) == 1);
  REQUIRE(std::holds_alternative<pop_clip_command>(commands.back()));
}
