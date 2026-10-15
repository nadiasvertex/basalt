#pragma once

#include <SDL3/SDL_video.h>

#include <expected>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "include/core/SkRect.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkSurface.h"

namespace example {

enum class backend { raster, gpu };

struct target_error {
  std::string message;
};

// Where finished pixels go. A target creates the persistent Skia surface the renderer draws into
// and moves its contents to the window.
class surface_target {
public:
  virtual ~surface_target() = default;

  [[nodiscard]] virtual auto kind() const -> backend = 0;
  // A short name for the status bar, such as "Metal".
  [[nodiscard]] virtual auto name() const -> std::string_view = 0;
  // A surface of the given device size, valid until the next call.
  [[nodiscard]] virtual auto make_surface(int width, int height) -> sk_sp<SkSurface> = 0;
  // Shows `surface`. `damage` is in device pixels; empty means the whole surface.
  [[nodiscard]] virtual auto present(SkSurface &surface, std::span<SkIRect const> damage)
      -> std::expected<void, target_error> = 0;
};

// Sets the SDL attributes a GPU window needs and returns the window flag to create it with. Call
// it before SDL_CreateWindow.
[[nodiscard]] auto configure_gpu_window() -> SDL_WindowFlags;

// Draws on the CPU and presents through the window's pixel surface. The window must not have
// been created with the GPU flag.
[[nodiscard]] auto make_raster_target(SDL_Window &window) -> std::unique_ptr<surface_target>;

// The platform's GPU target (Metal on macOS, OpenGL elsewhere) for a window created with
// `configure_gpu_window()`, or the reason one cannot be made.
[[nodiscard]] auto make_gpu_target(SDL_Window &window)
    -> std::expected<std::unique_ptr<surface_target>, target_error>;

} // namespace example
