#include <catch2/catch_test_macros.hpp>

#include "basalt/render_context.h"

#include <variant>

using namespace basalt;

TEST_CASE("render context records shape commands", "[paint]") {
  paint_commands commands;
  render_context context{commands};

  context.fill_rect({{1, 2}, {3, 4}}, {10, 20, 30, 255});
  context.stroke_rect({{5, 6}, {7, 8}}, {40, 50, 60, 255}, 2);
  context.fill_rounded_rect({{9, 10}, {11, 12}}, 3, {70, 80, 90, 255});
  context.stroke_rounded_rect({{13, 14}, {15, 16}}, 4, {100, 110, 120, 255}, 5);
  context.line({17, 18}, {19, 20}, {130, 140, 150, 255}, 6);

  REQUIRE(commands.size() == 5);
  CHECK(std::get<fill_rect_command>(commands[0]) ==
        fill_rect_command{{{1, 2}, {3, 4}}, {10, 20, 30, 255}});
  CHECK(std::get<stroke_rect_command>(commands[1]) ==
        stroke_rect_command{{{5, 6}, {7, 8}}, {40, 50, 60, 255}, 2});
  CHECK(std::get<fill_rounded_rect_command>(commands[2]) ==
        fill_rounded_rect_command{{{9, 10}, {11, 12}}, {70, 80, 90, 255}, 3});
  CHECK(std::get<stroke_rounded_rect_command>(commands[3]) ==
        stroke_rounded_rect_command{{{13, 14}, {15, 16}}, {100, 110, 120, 255}, 4, 5});
  CHECK(std::get<line_command>(commands[4]) ==
        line_command{{17, 18}, {19, 20}, {130, 140, 150, 255}, 6});
}

TEST_CASE("render context restores nested clips, transforms and layers") {
  paint_commands commands;
  render_context context{commands};

  {
    auto clip = context.clip({{0, 0}, {100, 100}});
    {
      auto transform = context.transform(affine_transform::translation(3, 4));
      {
        auto layer = context.layer(128);
        context.fill_rect({{1, 2}, {3, 4}}, {255, 0, 0, 255});
      }
    }
  }

  REQUIRE(commands.size() == 7);
  CHECK(std::holds_alternative<push_clip_command>(commands[0]));
  CHECK(std::holds_alternative<push_transform_command>(commands[1]));
  CHECK(std::holds_alternative<begin_layer_command>(commands[2]));
  CHECK(std::holds_alternative<fill_rect_command>(commands[3]));
  CHECK(std::holds_alternative<end_layer_command>(commands[4]));
  CHECK(std::holds_alternative<pop_transform_command>(commands[5]));
  CHECK(std::holds_alternative<pop_clip_command>(commands[6]));
}

TEST_CASE("moving a render scope closes it only once") {
  paint_commands commands;
  render_context context{commands};

  {
    auto first = context.layer();
    auto second = std::move(first);
  }

  REQUIRE(commands.size() == 2);
  CHECK(std::holds_alternative<begin_layer_command>(commands[0]));
  CHECK(std::holds_alternative<end_layer_command>(commands[1]));
}

TEST_CASE("render context records glyph runs", "[paint]") {
  paint_commands commands;
  render_context context{commands};
  auto run = std::make_shared<text::glyph_run const>();

  context.draw_glyph_run({10, 20}, run, {1, 2, 3, 255});

  REQUIRE(commands.size() == 1);
  auto const &command = std::get<draw_glyph_run_command>(commands[0]);
  CHECK(command.origin == point{10, 20});
  CHECK(command.run == run);
  CHECK(command.color == rgba{1, 2, 3, 255});
}
