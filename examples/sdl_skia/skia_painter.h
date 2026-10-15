#pragma once

#include "basalt/color.h"
#include "basalt/paint.h"

#include <cstddef>
#include <memory>

namespace example {

// BGRA, premultiplied: what Skia draws and what SDL calls ARGB8888 on little-endian machines.
struct pixel_view {
  void const *data = nullptr;
  int width = 0;
  int height = 0;
  std::size_t row_bytes = 0;
};

// Replays Basalt paint commands into a CPU raster surface. Skia types stay in the .cpp file.
class skia_painter {
public:
  skia_painter();
  skia_painter(skia_painter const &) = delete;
  auto operator=(skia_painter const &) -> skia_painter & = delete;
  ~skia_painter();

  // `width` and `height` are device pixels. Commands are in layout pixels and are scaled by
  // `scale`. The surface is cleared to `clear` before the commands run.
  auto render(basalt::paint_commands const &commands, int width, int height, float scale,
              basalt::rgba clear) -> void;

  [[nodiscard]] auto pixels() const -> pixel_view;

private:
  struct impl;
  std::unique_ptr<impl> impl_;
};

} // namespace example
