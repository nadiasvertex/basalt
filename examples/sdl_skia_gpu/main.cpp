// Draws a Basalt widget tree in an SDL window with Skia, repainting only the damaged area.
//
// The window uses the platform's GPU backend (Metal on macOS, OpenGL on Linux and Windows) when
// one can be created, and falls back to CPU rendering otherwise. Pass --software to skip the GPU
// and --log-damage to print how much of the window each frame repainted.

#include "basalt/basalt.h"

#include "damage_renderer.h"
#include "demo_ui.h"
#include "sdl_events.h"
#include "surface_target.h"
#include "window_metrics.h"

#include <SDL3/SDL.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <print>
#include <string_view>

namespace {

struct window_deleter {
  auto operator()(SDL_Window *window) const -> void { SDL_DestroyWindow(window); }
};
using window_ptr = std::unique_ptr<SDL_Window, window_deleter>;

constexpr auto window_title = "Basalt on SDL and Skia (damage rects)";
constexpr auto frame_interval_ms = 16;
constexpr auto base_window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;

struct options {
  bool software = false;
  bool log_damage = false;
};

auto parse_options(int argc, char **argv) -> options {
  auto result = options{};
  for (auto index = 1; index < argc; ++index) {
    auto const argument = std::string_view{argv[index]};
    if (argument == "--software") {
      result.software = true;
    } else if (argument == "--log-damage") {
      result.log_damage = true;
    } else {
      std::println(stderr, "Ignoring unknown argument {}", argument);
    }
  }
  return result;
}

auto create_window(SDL_WindowFlags extra) -> window_ptr {
  return window_ptr{SDL_CreateWindow(window_title, 760, 420, base_window_flags | extra)};
}

struct output {
  window_ptr window;
  std::unique_ptr<example::surface_target> target;
};

// A GPU window and target when possible. A window made for the GPU cannot present CPU pixels, so
// a failed GPU target destroys its window and the fallback makes a plain one.
auto create_output(bool software) -> output {
  if (!software) {
    auto window = create_window(example::configure_gpu_window());
    if (window != nullptr) {
      if (auto gpu = example::make_gpu_target(*window)) {
        return {std::move(window), std::move(*gpu)};
      } else {
        std::println(stderr, "GPU rendering is unavailable ({}); using the CPU.",
                     gpu.error().message);
      }
    } else {
      std::println(stderr, "Cannot create a GPU window ({}); using the CPU.", SDL_GetError());
    }
  }
  auto window = create_window(0);
  if (window == nullptr) {
    return {};
  }
  auto target = example::make_raster_target(*window);
  return {std::move(window), std::move(target)};
}

} // namespace

auto main(int argc, char **argv) -> int {
  auto const settings = parse_options(argc, argv);
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::println(stderr, "SDL_Init failed: {}", SDL_GetError());
    return 1;
  }
  auto out = create_output(settings.software);
  if (out.window == nullptr) {
    std::println(stderr, "Cannot create a window: {}", SDL_GetError());
    SDL_Quit();
    return 1;
  }
  auto &window = *out.window;
  SDL_StartTextInput(&window);

  auto fonts = basalt::text::font_manager{};
  if (!example::load_demo_fonts(fonts)) {
    std::println(stderr, "Cannot load the Noto Sans fonts from {}", BASALT_EXAMPLE_FONT_DIR);
    out.target.reset();
    SDL_Quit();
    return 1;
  }
  auto const shaper = basalt::text::text_shaper{fonts};

  auto tree = basalt::widget_tree{basalt::make_theme(basalt::color_scheme::light)};
  tree.set_text_shaper(&shaper);
  auto state = example::demo_state{};
  example::build_demo_ui(tree, state);
  auto router = basalt::event_router{tree};

  auto renderer = example::damage_renderer{std::move(out.target)};
  state.status->set_section_text(
      "renderer", std::string{"Skia "} + std::string{renderer.backend_name()} +
                      (renderer.active_backend() == example::backend::gpu ? " (GPU)" : ""));
  auto const metrics = example::query_metrics(window);
  example::apply_metrics(tree, metrics);
  renderer.set_metrics(metrics);

  while (state.running) {
    auto event = SDL_Event{};
    auto const animating = state.working || tree.needs_frame() || renderer.needs_full_redraw();
    auto available =
        animating ? SDL_WaitEventTimeout(&event, frame_interval_ms) : SDL_WaitEvent(&event);
    for (; available; available = SDL_PollEvent(&event)) {
      switch (event.type) {
      case SDL_EVENT_QUIT:
      case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        state.running = false;
        break;
      case SDL_EVENT_WINDOW_RESIZED:
      case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
      case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED: {
        auto const changed = example::query_metrics(window);
        example::apply_metrics(tree, changed);
        renderer.set_metrics(changed);
        break;
      }
      case SDL_EVENT_WINDOW_EXPOSED:
        renderer.request_full_redraw();
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

    auto const damage = tree.take_damage();
    auto repainted = 0.0F;
    if (auto const result = renderer.render(tree, damage, tree.theme().colors.background_default,
                                            &repainted);
        !result) {
      std::println(stderr, "Rendering failed: {}", result.error().message);
    } else if (settings.log_damage && repainted > 0) {
      std::println("repainted {:.1f}% of the window", repainted * 100);
    }
    tree.collect_garbage();
  }

  SDL_Quit();
  return 0;
}
