#include "damage_renderer.h"

#include "skia_replay.h"

#include "basalt/paint.h"
#include "basalt/widget_painter.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkRegion.h"

namespace example {
namespace {

// The device pixels covered by `area`, grown to whole pixels and kept inside the surface.
auto device_rect(basalt::rect area, float scale, int width, int height) -> SkIRect {
  auto const left = std::clamp(static_cast<int>(std::floor(area.left() * scale)), 0, width);
  auto const top = std::clamp(static_cast<int>(std::floor(area.top() * scale)), 0, height);
  auto const right = std::clamp(static_cast<int>(std::ceil(area.right() * scale)), 0, width);
  auto const bottom = std::clamp(static_cast<int>(std::ceil(area.bottom() * scale)), 0, height);
  return SkIRect::MakeLTRB(left, top, std::max(left, right), std::max(top, bottom));
}

} // namespace

struct damage_renderer::impl {
  std::unique_ptr<surface_target> target;
  sk_sp<SkSurface> surface;
  skia_replayer replayer;
  basalt::paint_commands commands;
  window_metrics metrics;
  bool full_redraw = true;
};

damage_renderer::damage_renderer(std::unique_ptr<surface_target> target)
    : impl_(std::make_unique<impl>()) {
  impl_->target = std::move(target);
}
damage_renderer::~damage_renderer() = default;

auto damage_renderer::active_backend() const -> backend { return impl_->target->kind(); }
auto damage_renderer::backend_name() const -> std::string_view { return impl_->target->name(); }

auto damage_renderer::set_metrics(window_metrics const &metrics) -> void {
  if (metrics == impl_->metrics) {
    return;
  }
  impl_->metrics = metrics;
  impl_->surface.reset();
  impl_->full_redraw = true;
}

auto damage_renderer::needs_full_redraw() const -> bool { return impl_->full_redraw; }
auto damage_renderer::request_full_redraw() -> void { impl_->full_redraw = true; }

auto damage_renderer::render(basalt::widget_tree const &tree, std::span<basalt::rect const> damage,
                             basalt::rgba clear, float *repainted)
    -> std::expected<void, target_error> {
  auto &state = *impl_;
  if (repainted != nullptr) {
    *repainted = 0;
  }
  if (!state.full_redraw && damage.empty()) {
    return {};
  }
  auto const width = state.metrics.pixel_width;
  auto const height = state.metrics.pixel_height;
  if (state.surface == nullptr) {
    state.surface = state.target->make_surface(width, height);
    state.full_redraw = true;
    if (state.surface == nullptr) {
      return std::unexpected(target_error{"could not create a render surface"});
    }
  }

  auto const viewport = basalt::rect{{0, 0}, state.metrics.viewport};
  auto areas = std::vector<SkIRect>{};
  auto paint_area = viewport;
  if (state.full_redraw) {
    areas.push_back(SkIRect::MakeWH(width, height));
  } else {
    auto bounds = std::optional<basalt::rect>{};
    for (auto const &area : damage) {
      auto const device = device_rect(area, state.metrics.scale, width, height);
      if (device.isEmpty()) {
        continue;
      }
      areas.push_back(device);
      bounds = bounds ? basalt::bounding_union(*bounds, area) : area;
    }
    if (areas.empty()) {
      return {};
    }
    paint_area = *bounds;
  }

  // Subtrees outside the damage are not visited; the canvas clip keeps the replay to the damage
  // itself, so the commands that reach outside it paint nothing.
  state.commands.clear();
  basalt::paint_tree(tree, state.commands, paint_area);

  auto region = SkRegion{};
  auto pixels = std::int64_t{0};
  for (auto const &area : areas) {
    region.op(area, SkRegion::kUnion_Op);
  }
  pixels = static_cast<std::int64_t>(region.getBounds().width()) * region.getBounds().height();
  auto &canvas = *state.surface->getCanvas();
  canvas.save();
  canvas.clipRegion(region);
  canvas.clear(SkColorSetARGB(clear.a, clear.r, clear.g, clear.b));
  canvas.scale(state.metrics.scale, state.metrics.scale);
  state.replayer.replay(canvas, state.commands);
  canvas.restore();

  auto result = state.target->present(
      *state.surface, state.full_redraw ? std::span<SkIRect const>{} : std::span{areas});
  if (result) {
    state.full_redraw = false;
  }
  if (repainted != nullptr) {
    *repainted = static_cast<float>(pixels) / static_cast<float>(std::int64_t{width} * height);
  }
  return result;
}

} // namespace example
