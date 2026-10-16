#pragma once

#include "basalt/basalt.h"

#include <chrono>
#include <memory>
#include <string>

namespace example {

// A host without a window, for the one-widget examples. It owns a light-theme tree, loads the
// bundled Noto Sans fonts, and simulates input. The widget under test goes into `content()`.
//
// The program writes a screenshot when its first argument is a file name. The file is a PAM image.
class example_host {
public:
  // `viewport` is in layout pixels. Throws std::runtime_error when the fonts are missing.
  explicit example_host(basalt::size viewport);
  example_host(example_host const &) = delete;
  auto operator=(example_host const &) -> example_host & = delete;
  ~example_host();

  [[nodiscard]] auto tree() -> basalt::widget_tree &;

  // The window content: a column with padding that gives each child its natural width.
  [[nodiscard]] auto content() -> basalt::box &;

  // The root widget. Popups and dialogs are shown here.
  [[nodiscard]] auto overlays() -> basalt::overlay_host &;

  // Runs layout and updates hover. Input helpers call it, so call it directly only after you
  // change the tree.
  auto frame() -> void;

  // Runs animation frames for `duration` of simulated time.
  auto advance(std::chrono::milliseconds duration) -> void;

  // Presses and releases the primary button in the middle of `target`.
  auto click(basalt::widget const &target) -> void;
  // `at` is in window coordinates.
  auto click_at(basalt::point at) -> void;
  auto move_pointer_to(basalt::widget const &target) -> void;
  auto press_key(basalt::key code, basalt::modifier modifiers = basalt::modifier::none) -> void;
  auto type(std::string text) -> void;

  // Writes the screenshot when `argv[1]` is set. Returns the process exit code.
  [[nodiscard]] auto finish(int argc, char **argv) -> int;

private:
  struct impl;
  std::unique_ptr<impl> impl_;
};

} // namespace example
