#pragma once

#include "basalt/paint.h"

#include <memory>

class SkCanvas;

namespace example {

// Replays Basalt paint commands onto a Skia canvas. It keeps the Skia typefaces and images it
// built for faces and images it has seen, so keep one for the life of the window.
class skia_replayer {
public:
  skia_replayer();
  skia_replayer(skia_replayer const &) = delete;
  auto operator=(skia_replayer const &) -> skia_replayer & = delete;
  ~skia_replayer();

  // Clips, transforms and layers pushed by the commands are restored before returning.
  auto replay(SkCanvas &canvas, basalt::paint_commands const &commands) -> void;

private:
  struct impl;
  std::unique_ptr<impl> impl_;
};

} // namespace example
