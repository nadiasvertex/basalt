# SDL and Skia example

This example shows a Basalt widget tree in an SDL 3 window, drawn with Skia on the CPU. It
demonstrates the whole path from input to pixels: SDL events become Basalt events, Basalt lays out
and paints the tree into paint commands, and Skia draws the commands into a bitmap that SDL puts on
screen.

## Build and run

```sh
just demo-sdl-skia
```

The recipe configures `examples/sdl_skia` into `.build/example` with Ninja, builds it in release
mode and starts it. The first build downloads and compiles Skia, which takes several minutes and
needs `git`, `python3`, `ninja` and network access. Later builds reuse it.

SDL 3.2 is taken from the system when `find_package(SDL3)` finds it, and is otherwise fetched at
the commit pinned in `cmake/example_pins.cmake`. Skia is always fetched at its pinned commit.

To build against an installed Basalt instead of this checkout, pass `-DBASALT_CONSUME=package` and
set `CMAKE_PREFIX_PATH` to the install prefix.

## What the demo shows

- A column of labels, a text field, a row of buttons, a progress bar and a status bar.
- Mouse, keyboard, Tab focus movement and text input, including IME composition.
- A light and dark theme switch that restyles the whole tree.
- Display scaling: the window is drawn at device resolution on high-DPI screens.
- An indeterminate progress bar that animates while "Start work" is active.

## Files

| File | Contents |
| ---- | -------- |
| `main.cpp` | Window, widget tree, event loop and frame schedule. |
| `sdl_events.cpp` | Translation of SDL input events to `basalt::event`. |
| `skia_painter.cpp` | Replay of `basalt::paint_commands` into a Skia raster surface. |
| `cmake/skia.cmake` | Builds Skia with GN and Ninja and exposes it as `skia::skia`. |

## Limits

The example repaints the whole window whenever anything is damaged and presents through SDL's
window surface. An application that needs partial updates or GPU rendering would use the damage
rects from `widget_tree::take_damage()` and a Skia GPU backend.
