// Metal target, used on macOS.
#include "surface_target.h"

#include <SDL3/SDL_metal.h>

#include "include/core/SkCanvas.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkImage.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/mtl/GrMtlBackendContext.h"
#include "include/gpu/ganesh/mtl/GrMtlDirectContext.h"
#include "include/gpu/ganesh/mtl/SkSurfaceMetal.h"

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

namespace example {
namespace {

class metal_target final : public surface_target {
public:
  explicit metal_target(SDL_Window &window) : window_(&window) {}

  [[nodiscard]] auto initialize() -> std::expected<void, target_error> {
    view_ = SDL_Metal_CreateView(window_);
    if (view_ == nullptr) {
      return std::unexpected(target_error{SDL_GetError()});
    }
    auto *const raw_layer = SDL_Metal_GetLayer(view_);
    if (raw_layer == nullptr) {
      return std::unexpected(target_error{"the window has no Metal layer"});
    }
    layer_ = (__bridge CAMetalLayer *)raw_layer;
    device_ = MTLCreateSystemDefaultDevice();
    if (device_ == nil) {
      return std::unexpected(target_error{"no Metal device is available"});
    }
    queue_ = [device_ newCommandQueue];
    if (queue_ == nil) {
      return std::unexpected(target_error{"could not create a Metal command queue"});
    }
    layer_.device = device_;
    layer_.pixelFormat = MTLPixelFormatBGRA8Unorm;
    // The drawable is only written by drawing the offscreen surface into it.
    layer_.framebufferOnly = NO;

    auto backend_context = GrMtlBackendContext{};
    backend_context.fDevice.retain((__bridge GrMTLHandle)device_);
    backend_context.fQueue.retain((__bridge GrMTLHandle)queue_);
    skia_ = GrDirectContexts::MakeMetal(backend_context);
    if (skia_ == nullptr) {
      return std::unexpected(target_error{"Skia could not create a Metal context"});
    }
    return {};
  }

  ~metal_target() override {
    if (skia_ != nullptr) {
      skia_->abandonContext();
    }
    if (view_ != nullptr) {
      SDL_Metal_DestroyView(view_);
    }
  }

  [[nodiscard]] auto kind() const -> backend override { return backend::gpu; }
  [[nodiscard]] auto name() const -> std::string_view override { return "Metal"; }

  [[nodiscard]] auto make_surface(int width, int height) -> sk_sp<SkSurface> override {
    layer_.drawableSize = CGSizeMake(width, height);
    return SkSurfaces::RenderTarget(
        skia_.get(), skgpu::Budgeted::kYes,
        SkImageInfo::Make(width, height, kBGRA_8888_SkColorType, kPremul_SkAlphaType), 0,
        kTopLeft_GrSurfaceOrigin, nullptr);
  }

  // A drawable's previous contents are not preserved, so the whole offscreen surface is copied
  // each time. Damage limits the repaint, not the present.
  [[nodiscard]] auto present(SkSurface &surface, std::span<SkIRect const>)
      -> std::expected<void, target_error> override {
    auto image = surface.makeImageSnapshot();
    if (image == nullptr) {
      return std::unexpected(target_error{"could not snapshot the Metal surface"});
    }
    GrMTLHandle drawable = nullptr;
    auto target = SkSurfaces::WrapCAMetalLayer(skia_.get(), (__bridge GrMTLHandle)layer_,
                                               kTopLeft_GrSurfaceOrigin, 1, kBGRA_8888_SkColorType,
                                               nullptr, nullptr, &drawable);
    if (target == nullptr) {
      return std::unexpected(target_error{"could not wrap the Metal layer"});
    }
    target->getCanvas()->drawImage(image, 0, 0);
    // Skia acquires the drawable when the surface is instantiated at flush, so it is only set
    // afterwards.
    skia_->flushAndSubmit(target.get());
    target.reset();
    if (drawable == nullptr) {
      return std::unexpected(target_error{"no Metal drawable is available"});
    }
    id<MTLCommandBuffer> commands = [queue_ commandBuffer];
    [commands presentDrawable:(__bridge id<CAMetalDrawable>)drawable];
    [commands commit];
    CFRelease(drawable);
    return {};
  }

private:
  SDL_Window *window_;
  SDL_MetalView view_ = nullptr;
  CAMetalLayer *layer_ = nil;
  id<MTLDevice> device_ = nil;
  id<MTLCommandQueue> queue_ = nil;
  sk_sp<GrDirectContext> skia_;
};

} // namespace

auto configure_gpu_window() -> SDL_WindowFlags { return SDL_WINDOW_METAL; }

auto make_gpu_target(SDL_Window &window)
    -> std::expected<std::unique_ptr<surface_target>, target_error> {
  auto target = std::make_unique<metal_target>(window);
  if (auto result = target->initialize(); !result) {
    return std::unexpected(result.error());
  }
  return target;
}

} // namespace example
