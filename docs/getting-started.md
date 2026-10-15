# Getting Started

This chapter builds a small program in seven steps. The program makes a widget tree with a label
and a button, sends a click to the button, and records paint commands. It has no window, so it
runs on any machine. The last section explains how to connect it to a window.

## Step 1: Add Basalt to your project

Add Basalt with CMake `FetchContent`. Use a release tag as `GIT_TAG`.

```cmake
cmake_minimum_required(VERSION 3.25)
project(hello LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(basalt
  GIT_REPOSITORY https://github.com/nadiasvertex/basalt.git
  GIT_TAG v0.3.0)
FetchContent_MakeAvailable(basalt)

add_executable(hello main.cpp)
target_link_libraries(hello PRIVATE basalt::basalt)
```

Basalt can also be used with `add_subdirectory` or `find_package`. See `README.md` for both.
The build downloads HarfBuzz at configure time.

## Step 2: Load fonts

Basalt does not search for fonts by itself. Add the fonts you want to a `font_manager`. Then
create a `text_shaper` that uses it.

```cpp
#include "basalt/basalt.h"

#include <chrono>
#include <cstdlib>
#include <format>
#include <memory>
#include <print>
#include <string>

auto main(int argc, char **argv) -> int {
  if (argc < 2) {
    std::println(stderr, "usage: hello <font file>");
    return EXIT_FAILURE;
  }

  auto fonts = basalt::text::font_manager{};
  if (fonts.add_file(argv[1]) == 0) {
    std::println(stderr, "Cannot load the font {}", argv[1]);
    return EXIT_FAILURE;
  }
  auto const shaper = basalt::text::text_shaper{fonts};
```

`add_file` returns the number of faces that it added. Use `add_system_fonts` to add the fonts of
the operating system. The `font_manager` must stay alive while the shaper is in use. Do not shape
on two threads with the same `font_manager` at the same time.

## Step 3: Create the widget tree

Make a `widget_tree` with a theme. Give it the shaper and the viewport size.

```cpp
  auto tree = basalt::widget_tree{basalt::make_theme(basalt::color_scheme::light)};
  tree.set_text_shaper(&shaper);
  tree.set_viewport({480, 240});
```

The viewport is in layout pixels. In a real program, you set it from the window size. You also
set the display scale with `set_scale`.

## Step 4: Add widgets

Make a vertical `box` the root. Add a `label` and a `button` to it.

```cpp
  auto clicks = 0;
  auto &root = tree.emplace_root<basalt::box>(basalt::axis::vertical);
  root.set_padding(basalt::length_insets::uniform(basalt::fixed(16)));
  root.set_gap(basalt::fixed(8));
  auto &status = root.emplace_child<basalt::label>("Clicks: 0");
  auto &press = root.emplace_child<basalt::button>("Count a click");
  press.set_variant(basalt::button_variant::primary);
  press.set_on_press([&] { status.set_text(std::format("Clicks: {}", ++clicks)); });
```

`emplace_root` and `emplace_child` construct the widget and return a reference to it. The tree
owns the widgets. The references stay valid until you remove the widget.

## Step 5: Make the event router

An `event_router` sends events to widgets in one tree. The router must not outlive the tree.

```cpp
  auto router = basalt::event_router{tree};
  auto commands = basalt::paint_commands{};
```

## Step 6: Run the frame cycle

Put the cycle in a function. Call it each time the tree needs a frame.

```cpp
  auto const frame = [&] {
    tree.advance_animations(std::chrono::steady_clock::now().time_since_epoch());
    tree.update_layout();
    router.update_hover();
    auto const damage = tree.take_damage();
    if (!damage.empty()) {
      commands.clear();
      basalt::paint_tree(tree, commands);
    }
    tree.collect_garbage();
  };
```

The order is important. Layout must run before damage is taken. Damage must be taken before
painting. See the [frame cycle](architecture.md#frame-cycle) for the reason.

## Step 7: Send input and read the result

Run one frame. Then send a pointer press and release to the center of the button. In a real
program, the host translates window events into `basalt::event` values.

```cpp
  frame();
  auto const center = press.bounds_in_root().center();
  for (auto const action : {basalt::pointer_action::down, basalt::pointer_action::up}) {
    router.dispatch(basalt::pointer_event{.action = action,
                                          .position = center,
                                          .button = basalt::pointer_button::primary});
  }
  frame();

  std::println("{} ({} paint commands)", status.text(), commands.size());
  return clicks == 1 ? EXIT_SUCCESS : EXIT_FAILURE;
}
```

Build and run the program with the path of a font file. For example, use a Noto Sans file from
`tests/data/fonts`. The program prints `Clicks: 1` and the number of paint commands.

## Connect to a window

A program with a window has the same parts. These changes make it a full application:

1. Create a window with your platform library.
2. Translate each platform event into a `basalt::event`. Pass it to `router.dispatch`.
3. On a resize or scale change, call `tree.set_viewport` and `tree.set_scale`.
4. Call `tree.set_frame_request_handler` to wake the event loop. Run `frame` in the loop.
5. Draw `commands` with your renderer, in order. Multiply layout pixels by the display scale.

The `examples/sdl_skia` directory has a complete host. Build and run it with `just demo-sdl-skia`.
The first build compiles Skia, so it takes a long time.

## Where to go next

- Read [Architecture](architecture.md) for the rules of each part.
- Open `include/basalt/widgets/` to see the available widgets and their options.
- Open `examples/common/demo_ui.cpp` for more widgets in use.
- Read `spec/CONVENTIONS.md` before you change the library.
