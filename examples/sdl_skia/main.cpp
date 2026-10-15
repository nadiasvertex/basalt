// Draws a small Basalt widget tree in an SDL window with Skia.
//
// Basalt turns the tree into paint commands. This file owns the window, the event loop and the
// frame schedule; skia_painter.cpp turns paint commands into pixels; sdl_events.cpp turns SDL
// input into Basalt events.

#include "basalt/basalt.h"

#include "demo_ui.h"
#include "sdl_events.h"
#include "skia_painter.h"
#include "window_metrics.h"

#include <SDL3/SDL.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <memory>
#include <print>
#include <string>

namespace {

struct window_deleter {
  auto operator()(SDL_Window *window) const -> void { SDL_DestroyWindow(window); }
};
struct surface_deleter {
  auto operator()(SDL_Surface *surface) const -> void { SDL_DestroySurface(surface); }
};

constexpr auto window_title = "Basalt on SDL and Skia";
constexpr auto frame_interval_ms = 16;

auto present(SDL_Window &window, example::pixel_view const &pixels) -> bool {
  auto *const target = SDL_GetWindowSurface(&window);
  if (target == nullptr || pixels.data == nullptr) {
    return false;
  }
  // ARGB8888 is BGRA in memory on little-endian machines, which is what Skia drew.
  auto const source = std::unique_ptr<SDL_Surface, surface_deleter>{
      SDL_CreateSurfaceFrom(pixels.width, pixels.height, SDL_PIXELFORMAT_ARGB8888,
                            const_cast<void *>(pixels.data), static_cast<int>(pixels.row_bytes))};
  if (source == nullptr) {
    return false;
  }
  SDL_SetSurfaceBlendMode(source.get(), SDL_BLENDMODE_NONE);
  return SDL_BlitSurface(source.get(), nullptr, target, nullptr) &&
         SDL_UpdateWindowSurface(&window);
}

} // namespace

auto main() -> int {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::println(stderr, "SDL_Init failed: {}", SDL_GetError());
    return 1;
  }
  auto const window = std::unique_ptr<SDL_Window, window_deleter>{SDL_CreateWindow(
      window_title, 760, 420, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY)};
  if (window == nullptr) {
    std::println(stderr, "SDL_CreateWindow failed: {}", SDL_GetError());
    SDL_Quit();
    return 1;
  }
  SDL_StartTextInput(window.get());

  auto fonts = basalt::text::font_manager{};
  if (!example::load_demo_fonts(fonts)) {
    std::println(stderr, "Cannot load the Noto Sans fonts from {}", BASALT_EXAMPLE_FONT_DIR);
    SDL_Quit();
    return 1;
  }
  auto const shaper = basalt::text::text_shaper{fonts};

  auto tree = basalt::widget_tree{basalt::make_theme(basalt::color_scheme::light)};
  tree.set_text_shaper(&shaper);
  auto state = example::demo_state{};
  example::build_demo_ui(tree, state);
  state.status->set_section_text("renderer", "Skia raster (CPU)");
  auto router = basalt::event_router{tree};
  example::apply_metrics(tree, example::query_metrics(*window));

  auto painter = example::skia_painter{};
  auto commands = basalt::paint_commands{};
  auto redraw = true;

  while (state.running) {
    auto event = SDL_Event{};
    auto const animating = state.working || tree.needs_frame() || redraw;
    auto available = animating ? SDL_WaitEventTimeout(&event, frame_interval_ms)
                               : SDL_WaitEvent(&event);
    for (; available; available = SDL_PollEvent(&event)) {
      switch (event.type) {
      case SDL_EVENT_QUIT:
      case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        state.running = false;
        break;
      case SDL_EVENT_WINDOW_RESIZED:
      case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
      case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
        example::apply_metrics(tree, example::query_metrics(*window));
        redraw = true;
        break;
      case SDL_EVENT_WINDOW_EXPOSED:
        redraw = true;
        break;
      default:
        if (auto const translated = example::translate_event(event)) {
          router.dispatch(*translated);
        }
        break;
      }
    }

    if (state.working) {
      state.progress->tick(example::progress_step);
    }
    tree.advance_animations(std::chrono::steady_clock::now().time_since_epoch());
    tree.update_layout();
    router.update_hover();

    // Damage is not used to limit painting: the whole viewport is repainted when anything changed.
    auto const damaged = !tree.take_damage().empty();
    if (damaged || redraw) {
      commands.clear();
      basalt::paint_tree(tree, commands);
      auto const metrics = example::query_metrics(*window);
      painter.render(commands, metrics.pixel_width, metrics.pixel_height, metrics.scale,
                     tree.theme().colors.background_default);
      if (!present(*window, painter.pixels())) {
        std::println(stderr, "Presenting failed: {}", SDL_GetError());
      }
      redraw = false;
    }
    tree.collect_garbage();
  }

  SDL_Quit();
  return 0;
}
