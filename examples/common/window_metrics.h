#pragma once

#include "basalt/geometry.h"
#include "basalt/widget_tree.h"

#include <SDL3/SDL_video.h>

#include <algorithm>

namespace example {

// The window's size in layout pixels and the device pixels per layout pixel.
struct window_metrics {
  basalt::size viewport;
  int pixel_width = 1;
  int pixel_height = 1;
  float scale = 1;

  [[nodiscard]] friend auto operator==(window_metrics const &, window_metrics const &)
      -> bool = default;
};

[[nodiscard]] inline auto query_metrics(SDL_Window &window) -> window_metrics {
  auto width = 0;
  auto height = 0;
  auto pixel_width = 0;
  auto pixel_height = 0;
  SDL_GetWindowSize(&window, &width, &height);
  SDL_GetWindowSizeInPixels(&window, &pixel_width, &pixel_height);
  auto metrics = window_metrics{};
  metrics.viewport = {static_cast<float>(width), static_cast<float>(height)};
  metrics.pixel_width = std::max(pixel_width, 1);
  metrics.pixel_height = std::max(pixel_height, 1);
  metrics.scale = width > 0 ? static_cast<float>(pixel_width) / static_cast<float>(width) : 1.0F;
  return metrics;
}

inline auto apply_metrics(basalt::widget_tree &tree, window_metrics const &metrics) -> void {
  auto scale = tree.scale();
  scale.display = metrics.scale;
  tree.set_scale(scale);
  tree.set_viewport(metrics.viewport);
}

} // namespace example
