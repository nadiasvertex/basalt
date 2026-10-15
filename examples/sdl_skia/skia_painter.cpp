#include "skia_painter.h"

#include "skia_replay.h"

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkSurface.h"

namespace example {

struct skia_painter::impl {
  sk_sp<SkSurface> surface;
  skia_replayer replayer;
};

skia_painter::skia_painter() : impl_(std::make_unique<impl>()) {}
skia_painter::~skia_painter() = default;

auto skia_painter::render(basalt::paint_commands const &commands, int width, int height,
                          float scale, basalt::rgba clear) -> void {
  if (impl_->surface == nullptr || impl_->surface->width() != width ||
      impl_->surface->height() != height) {
    impl_->surface = SkSurfaces::Raster(
        SkImageInfo::Make(width, height, kBGRA_8888_SkColorType, kPremul_SkAlphaType));
  }
  auto &canvas = *impl_->surface->getCanvas();
  canvas.save();
  canvas.clear(SkColorSetARGB(clear.a, clear.r, clear.g, clear.b));
  canvas.scale(scale, scale);
  impl_->replayer.replay(canvas, commands);
  canvas.restore();
}

auto skia_painter::pixels() const -> pixel_view {
  auto pixmap = SkPixmap{};
  if (impl_->surface == nullptr || !impl_->surface->peekPixels(&pixmap)) {
    return {};
  }
  return {pixmap.addr(), pixmap.width(), pixmap.height(), pixmap.rowBytes()};
}

} // namespace example
