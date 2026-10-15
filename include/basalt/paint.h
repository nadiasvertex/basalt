#pragma once

#include "basalt/color.h"
#include "basalt/geometry.h"
#include "basalt/image.h"
#include "basalt/text/glyph_run.h"

#include <cstdint>
#include <memory>
#include <variant>
#include <vector>

namespace basalt {

struct fill_rect_command {
  rect bounds;
  rgba color;

  [[nodiscard]] friend constexpr auto operator==(fill_rect_command const &,
                                                 fill_rect_command const &) -> bool = default;
};

struct stroke_rect_command {
  rect bounds;
  rgba color;
  float width = 0;

  [[nodiscard]] friend constexpr auto operator==(stroke_rect_command const &,
                                                 stroke_rect_command const &) -> bool = default;
};

struct fill_rounded_rect_command {
  rect bounds;
  rgba color;
  float radius = 0;

  [[nodiscard]] friend constexpr auto operator==(fill_rounded_rect_command const &,
                                                 fill_rounded_rect_command const &)
      -> bool = default;
};

struct stroke_rounded_rect_command {
  rect bounds;
  rgba color;
  float radius = 0;
  float width = 0;

  [[nodiscard]] friend constexpr auto operator==(stroke_rounded_rect_command const &,
                                                 stroke_rounded_rect_command const &)
      -> bool = default;
};

struct line_command {
  point from;
  point to;
  rgba color;
  float width = 0;

  [[nodiscard]] friend constexpr auto operator==(line_command const &, line_command const &)
      -> bool = default;
};

// Draws shaped glyphs with their baseline start at `origin`.
struct draw_glyph_run_command {
  point origin;
  text::glyph_run_ptr run;
  rgba color;

  [[nodiscard]] friend auto operator==(draw_glyph_run_command const &,
                                       draw_glyph_run_command const &) -> bool = default;
};

// Draws the whole image scaled to fill `bounds`.
struct draw_image_command {
  rect bounds;
  raster_image_ptr image;

  [[nodiscard]] friend auto operator==(draw_image_command const &, draw_image_command const &)
      -> bool = default;
};

struct push_clip_command {
  rect bounds;

  [[nodiscard]] friend constexpr auto operator==(push_clip_command const &,
                                                 push_clip_command const &) -> bool = default;
};

struct pop_clip_command {
  [[nodiscard]] friend constexpr auto operator==(pop_clip_command const &, pop_clip_command const &)
      -> bool = default;
};

struct push_transform_command {
  affine_transform transform;

  [[nodiscard]] friend constexpr auto operator==(push_transform_command const &,
                                                 push_transform_command const &) -> bool = default;
};

struct pop_transform_command {
  [[nodiscard]] friend constexpr auto operator==(pop_transform_command const &,
                                                 pop_transform_command const &) -> bool = default;
};

struct begin_layer_command {
  std::uint8_t opacity = 255;

  [[nodiscard]] friend constexpr auto operator==(begin_layer_command const &,
                                                 begin_layer_command const &) -> bool = default;
};

struct end_layer_command {
  [[nodiscard]] friend constexpr auto operator==(end_layer_command const &,
                                                 end_layer_command const &) -> bool = default;
};

using paint_command =
    std::variant<fill_rect_command, stroke_rect_command, fill_rounded_rect_command,
                 stroke_rounded_rect_command, line_command, draw_glyph_run_command,
                 draw_image_command, push_clip_command, pop_clip_command, push_transform_command,
                 pop_transform_command, begin_layer_command, end_layer_command>;
using paint_commands = std::vector<paint_command>;

} // namespace basalt
