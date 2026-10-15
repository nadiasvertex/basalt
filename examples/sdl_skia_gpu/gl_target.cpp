// OpenGL target, used on Linux and Windows.
#include "surface_target.h"

#include <SDL3/SDL_opengl.h>

#include "include/core/SkCanvas.h"
#include "include/core/SkImage.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLAssembleInterface.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "include/gpu/ganesh/gl/GrGLInterface.h"
#include "include/gpu/ganesh/gl/GrGLTypes.h"

namespace example {
namespace {

constexpr auto gl_rgba8 = 0x8058; // GL_RGBA8

struct context_deleter {
  auto operator()(SDL_GLContextState *context) const -> void { SDL_GL_DestroyContext(context); }
};

class gl_target final : public surface_target {
public:
  explicit gl_target(SDL_Window &window) : window_(&window) {}

  [[nodiscard]] auto initialize() -> std::expected<void, target_error> {
    context_.reset(SDL_GL_CreateContext(window_));
    if (context_ == nullptr) {
      return std::unexpected(target_error{SDL_GetError()});
    }
    SDL_GL_MakeCurrent(window_, context_.get());
    SDL_GL_SetSwapInterval(1);
    auto const interface =
        GrGLMakeAssembledInterface(nullptr, [](void *, char const *name) -> GrGLFuncPtr {
          return reinterpret_cast<GrGLFuncPtr>(SDL_GL_GetProcAddress(name));
        });
    if (interface == nullptr) {
      return std::unexpected(target_error{"could not load the OpenGL interface"});
    }
    skia_ = GrDirectContexts::MakeGL(interface);
    if (skia_ == nullptr) {
      return std::unexpected(target_error{"Skia could not create an OpenGL context"});
    }
    return {};
  }

  ~gl_target() override {
    if (skia_ != nullptr) {
      skia_->abandonContext();
    }
  }

  [[nodiscard]] auto kind() const -> backend override { return backend::gpu; }
  [[nodiscard]] auto name() const -> std::string_view override { return "OpenGL"; }

  [[nodiscard]] auto make_surface(int width, int height) -> sk_sp<SkSurface> override {
    return SkSurfaces::RenderTarget(
        skia_.get(), skgpu::Budgeted::kYes,
        SkImageInfo::Make(width, height, kRGBA_8888_SkColorType, kPremul_SkAlphaType), 0,
        kTopLeft_GrSurfaceOrigin, nullptr);
  }

  // The back buffer is not guaranteed to keep the previous frame, so the whole offscreen surface
  // is copied each time. Damage limits the repaint, not the present.
  [[nodiscard]] auto present(SkSurface &surface, std::span<SkIRect const>)
      -> std::expected<void, target_error> override {
    SDL_GL_MakeCurrent(window_, context_.get());
    auto image = surface.makeImageSnapshot();
    if (image == nullptr) {
      return std::unexpected(target_error{"could not snapshot the OpenGL surface"});
    }
    auto framebuffer = GrGLFramebufferInfo{};
    framebuffer.fFBOID = 0;
    framebuffer.fFormat = gl_rgba8;
    auto const target =
        GrBackendRenderTargets::MakeGL(surface.width(), surface.height(), 0, 8, framebuffer);
    auto screen =
        SkSurfaces::WrapBackendRenderTarget(skia_.get(), target, kBottomLeft_GrSurfaceOrigin,
                                            kRGBA_8888_SkColorType, nullptr, nullptr);
    if (screen == nullptr) {
      return std::unexpected(target_error{"could not wrap the OpenGL framebuffer"});
    }
    screen->getCanvas()->drawImage(image, 0, 0);
    skia_->flushAndSubmit(screen.get());
    if (!SDL_GL_SwapWindow(window_)) {
      return std::unexpected(target_error{SDL_GetError()});
    }
    return {};
  }

private:
  SDL_Window *window_;
  std::unique_ptr<SDL_GLContextState, context_deleter> context_;
  sk_sp<GrDirectContext> skia_;
};

} // namespace

auto configure_gpu_window() -> SDL_WindowFlags {
  // Skia's wrapped framebuffer needs a stencil buffer.
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
  return SDL_WINDOW_OPENGL;
}

auto make_gpu_target(SDL_Window &window)
    -> std::expected<std::unique_ptr<surface_target>, target_error> {
  auto target = std::make_unique<gl_target>(window);
  if (auto result = target->initialize(); !result) {
    return std::unexpected(result.error());
  }
  return target;
}

} // namespace example
