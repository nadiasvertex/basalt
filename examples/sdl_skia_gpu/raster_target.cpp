#include "surface_target.h"

#include <algorithm>
#include <vector>

#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"

namespace example {
namespace {

struct surface_deleter {
  auto operator()(SDL_Surface *surface) const -> void { SDL_DestroySurface(surface); }
};

class raster_target final : public surface_target {
public:
  explicit raster_target(SDL_Window &window) : window_(&window) {}

  [[nodiscard]] auto kind() const -> backend override { return backend::raster; }
  [[nodiscard]] auto name() const -> std::string_view override { return "raster (CPU)"; }

  [[nodiscard]] auto make_surface(int width, int height) -> sk_sp<SkSurface> override {
    return SkSurfaces::Raster(
        SkImageInfo::Make(width, height, kBGRA_8888_SkColorType, kPremul_SkAlphaType));
  }

  [[nodiscard]] auto present(SkSurface &surface, std::span<SkIRect const> damage)
      -> std::expected<void, target_error> override {
    auto pixmap = SkPixmap{};
    if (!surface.peekPixels(&pixmap)) {
      return std::unexpected(target_error{"the raster surface has no readable pixels"});
    }
    auto *const target = SDL_GetWindowSurface(window_);
    if (target == nullptr) {
      return std::unexpected(target_error{SDL_GetError()});
    }
    // ARGB8888 is BGRA in memory on little-endian machines, which is what Skia drew.
    auto const source = std::unique_ptr<SDL_Surface, surface_deleter>{SDL_CreateSurfaceFrom(
        pixmap.width(), pixmap.height(), SDL_PIXELFORMAT_ARGB8888,
        const_cast<void *>(pixmap.addr()), static_cast<int>(pixmap.rowBytes()))};
    if (source == nullptr) {
      return std::unexpected(target_error{SDL_GetError()});
    }
    SDL_SetSurfaceBlendMode(source.get(), SDL_BLENDMODE_NONE);

    auto rects = std::vector<SDL_Rect>{};
    for (auto const &area : damage) {
      auto const left = std::clamp(area.left(), 0, target->w);
      auto const top = std::clamp(area.top(), 0, target->h);
      auto const right = std::clamp(area.right(), 0, target->w);
      auto const bottom = std::clamp(area.bottom(), 0, target->h);
      if (right > left && bottom > top) {
        rects.push_back({left, top, right - left, bottom - top});
      }
    }
    if (damage.empty()) {
      rects.push_back({0, 0, target->w, target->h});
    }
    for (auto &area : rects) {
      if (!SDL_BlitSurface(source.get(), &area, target, &area)) {
        return std::unexpected(target_error{SDL_GetError()});
      }
    }
    if (!rects.empty() &&
        !SDL_UpdateWindowSurfaceRects(window_, rects.data(), static_cast<int>(rects.size()))) {
      return std::unexpected(target_error{SDL_GetError()});
    }
    return {};
  }

private:
  SDL_Window *window_;
};

} // namespace

auto make_raster_target(SDL_Window &window) -> std::unique_ptr<surface_target> {
  return std::make_unique<raster_target>(window);
}

} // namespace example
