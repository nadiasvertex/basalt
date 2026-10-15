# SDL and Skia example with damage rects and GPU rendering

This example draws the same widget tree as `../sdl_skia`, but repaints only the area Basalt reports
as damaged and uses the platform's GPU backend when one can be created. It falls back to CPU
rendering when the GPU is unavailable or when `--software` is passed.

## Build and run

```sh
just demo-sdl-skia-gpu
just demo-sdl-skia-gpu --log-damage
just demo-sdl-skia-gpu --software
```

The first build downloads and compiles a Skia with its GPU backend, separate from the CPU-only
build used by `../sdl_skia`. It takes several minutes and needs `git`, `python3`, `ninja` and
network access. SDL is handled as in `../sdl_skia`.

## Backends

| Platform | GPU backend | Fallback |
| -------- | ----------- | -------- |
| macOS | Metal | CPU raster |
| Linux | OpenGL | CPU raster |
| Windows | OpenGL | CPU raster |

The status bar names the backend in use. `--log-damage` prints the share of the window repainted
by each frame, which stays small for hover and focus changes and reaches 100% after a resize.

Only the macOS build has been run. The Linux and Windows paths are written but untested.

## How damage is used

`widget_tree::take_damage()` returns rects in layout pixels. `damage_renderer` converts them to
device pixels, asks `paint_tree` for commands inside their bounds, clips the canvas to the damaged
region and replays the commands onto a Skia surface that persists between frames. A resize or scale
change discards the surface and repaints everything.

The persistent surface limits repainting on every backend. Presenting differs:

- The CPU target blits only the damaged rects to the window.
- The GPU targets copy the whole surface to the back buffer or drawable each frame, because neither
  keeps the previous frame's contents.

## Files

| File | Contents |
| ---- | -------- |
| `main.cpp` | Window creation with GPU fallback, event loop and frame schedule. |
| `damage_renderer.cpp` | Persistent surface, damage-to-device conversion and clipped replay. |
| `surface_target.h` | The interface each backend implements. |
| `raster_target.cpp` | CPU target, presents through the window surface. |
| `metal_target.mm` | Metal target for macOS. |
| `gl_target.cpp` | OpenGL target for Linux and Windows. |

## Limits

- Windows uses OpenGL, not Direct3D. Skia has a Direct3D backend, but it needs a device, swap
  chain and fence handling that the example does not carry.
- A window made for the GPU cannot present CPU pixels, so when GPU setup fails the window is
  destroyed and recreated.
