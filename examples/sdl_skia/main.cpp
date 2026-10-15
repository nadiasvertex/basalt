// Draws a small Basalt widget tree in an SDL window with Skia.
//
// Basalt turns the tree into paint commands. This file owns the window, the event loop and the
// frame schedule; skia_painter.cpp turns paint commands into pixels; sdl_events.cpp turns SDL
// input into Basalt events.

#include "basalt/basalt.h"

#include "sdl_events.h"
#include "skia_painter.h"

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
// Share of the progress bar track that one frame of the demo operation fills.
constexpr auto progress_step = 0.004F;

// The widgets the event handlers need to reach.
struct demo_state {
  bool running = true;
  bool working = false;
  bool dark = false;
  int clicks = 0;
  basalt::label *click_label = nullptr;
  basalt::label *echo_label = nullptr;
  basalt::progress_bar *progress = nullptr;
  basalt::button *work_button = nullptr;
};

auto build_ui(basalt::widget_tree &tree, demo_state &state) -> void {
  using namespace basalt;

  auto &root = tree.emplace_root<box>(axis::vertical);
  root.set_padding(length_insets::uniform(fixed(24)));
  root.set_gap(fixed(16));

  root.add_child(std::make_unique<label>("Basalt on SDL and Skia", text_role::heading_large));
  auto &intro = root.emplace_child<label>(
      "Widgets, layout and input come from Basalt. SDL owns the window; Skia draws the pixels.");
  intro.set_tone(text_tone::subdued);

  auto &field = root.emplace_child<text_field>();
  field.set_placeholder("Type here");
  field.set_accessible_name("Demo text");
  state.echo_label = &root.emplace_child<label>("You typed: ");
  field.set_on_change(
      [&state](std::string const &text) { state.echo_label->set_text("You typed: " + text); });

  root.add_child(std::make_unique<separator>());

  auto &buttons = root.emplace_child<box>(axis::horizontal);
  buttons.set_gap(fixed(8));
  buttons.set_cross_alignment(alignment::center);

  auto &count = buttons.emplace_child<button>("Count a click");
  count.set_variant(button_variant::primary);
  state.click_label = &buttons.emplace_child<label>("Clicks: 0");
  count.set_on_press([&state] {
    state.click_label->set_text(std::format("Clicks: {}", ++state.clicks));
  });

  state.work_button = &buttons.emplace_child<button>("Start work");
  state.work_button->set_on_press([&state] {
    state.working = !state.working;
    state.work_button->set_label(state.working ? "Stop work" : "Start work");
    state.progress->set_fraction(state.working ? std::nullopt : std::optional<float>{0});
  });

  auto &theme_button = buttons.emplace_child<button>("Toggle theme");
  theme_button.set_variant(button_variant::tertiary);
  theme_button.set_on_press([&state, &tree] {
    state.dark = !state.dark;
    tree.set_theme(make_theme(state.dark ? color_scheme::dark : color_scheme::light));
  });

  auto &quit = buttons.emplace_child<button>("Quit");
  quit.set_variant(button_variant::danger);
  quit.set_on_press([&state] { state.running = false; });

  state.progress = &root.emplace_child<progress_bar>(std::optional<float>{0});

  // Takes the space left over so the status bar sits at the bottom edge.
  root.add(std::make_unique<box>(), box_item{.grow = 1});

  auto &status = root.emplace_child<status_bar>();
  status.add_section("renderer", "Skia raster", 1);
  status.add_section("hint", "Tab moves focus; Space or Enter presses a button");
}

// The window's size in layout pixels and the device pixels per layout pixel.
struct window_metrics {
  basalt::size viewport;
  int pixel_width = 1;
  int pixel_height = 1;
  float scale = 1;
};

auto query_metrics(SDL_Window &window) -> window_metrics {
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

auto apply_metrics(basalt::widget_tree &tree, window_metrics const &metrics) -> void {
  auto scale = tree.scale();
  scale.display = metrics.scale;
  tree.set_scale(scale);
  tree.set_viewport(metrics.viewport);
}

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

auto load_fonts(basalt::text::font_manager &fonts) -> bool {
  auto const directory = std::filesystem::path{BASALT_EXAMPLE_FONT_DIR};
  auto added = std::size_t{0};
  for (auto const *name : {"NotoSans-VariableFont_wdth,wght.ttf",
                           "NotoSans-Italic-VariableFont_wdth,wght.ttf"}) {
    added += fonts.add_file(directory / name);
  }
  // System fonts supply the monospace family and scripts Noto Sans lacks.
  fonts.add_system_fonts();
  return added > 0;
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
  if (!load_fonts(fonts)) {
    std::println(stderr, "Cannot load the Noto Sans fonts from {}", BASALT_EXAMPLE_FONT_DIR);
    SDL_Quit();
    return 1;
  }
  auto const shaper = basalt::text::text_shaper{fonts};

  auto tree = basalt::widget_tree{basalt::make_theme(basalt::color_scheme::light)};
  tree.set_text_shaper(&shaper);
  auto state = demo_state{};
  build_ui(tree, state);
  auto router = basalt::event_router{tree};
  apply_metrics(tree, query_metrics(*window));

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
        apply_metrics(tree, query_metrics(*window));
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
      state.progress->tick(progress_step);
    }
    tree.advance_animations(std::chrono::steady_clock::now().time_since_epoch());
    tree.update_layout();
    router.update_hover();

    // Damage is not used to limit painting: the whole viewport is repainted when anything changed.
    auto const damaged = !tree.take_damage().empty();
    if (damaged || redraw) {
      commands.clear();
      basalt::paint_tree(tree, commands);
      auto const metrics = query_metrics(*window);
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
