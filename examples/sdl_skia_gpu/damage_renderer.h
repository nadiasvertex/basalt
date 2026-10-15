#pragma once

#include "surface_target.h"
#include "window_metrics.h"

#include "basalt/color.h"
#include "basalt/geometry.h"
#include "basalt/widget_tree.h"

#include <expected>
#include <memory>
#include <span>
#include <string_view>

namespace example {

// Paints only the damaged part of a widget tree into a persistent Skia surface, then presents it.
//
// The surface survives between frames, so pixels outside the damage are reused. A change of
// device size or scale discards it and the next frame repaints everything.
class damage_renderer {
public:
  explicit damage_renderer(std::unique_ptr<surface_target> target);
  damage_renderer(damage_renderer const &) = delete;
  auto operator=(damage_renderer const &) -> damage_renderer & = delete;
  ~damage_renderer();

  [[nodiscard]] auto active_backend() const -> backend;
  [[nodiscard]] auto backend_name() const -> std::string_view;

  auto set_metrics(window_metrics const &metrics) -> void;
  // True until a frame that repaints everything has been presented.
  [[nodiscard]] auto needs_full_redraw() const -> bool;
  auto request_full_redraw() -> void;

  // Repaints `damage` (layout pixels, from `widget_tree::take_damage()`), or everything when a
  // full redraw is pending, and presents. Nothing happens when there is no damage and no
  // pending full redraw. `repainted` receives the fraction of the surface that was repainted.
  [[nodiscard]] auto render(basalt::widget_tree const &tree, std::span<basalt::rect const> damage,
                            basalt::rgba clear, float *repainted = nullptr)
      -> std::expected<void, target_error>;

private:
  struct impl;
  std::unique_ptr<impl> impl_;
};

} // namespace example
