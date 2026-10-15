#pragma once

#include "basalt/basalt.h"

namespace example {

// The widgets the event handlers of the demo UI need to reach, and what the application reads
// back from them.
struct demo_state {
  bool running = true;
  bool working = false;
  bool dark = false;
  int clicks = 0;
  basalt::label *click_label = nullptr;
  basalt::label *echo_label = nullptr;
  basalt::progress_bar *progress = nullptr;
  basalt::button *work_button = nullptr;
  basalt::status_bar *status = nullptr;
};

// Share of the progress bar track that one frame of the demo operation fills.
inline constexpr auto progress_step = 0.004F;

// Fills `tree` with the demo window. `state` must outlive the tree.
auto build_demo_ui(basalt::widget_tree &tree, demo_state &state) -> void;

// Loads the bundled Noto Sans files plus the system fonts. False when the bundled files are
// missing.
[[nodiscard]] auto load_demo_fonts(basalt::text::font_manager &fonts) -> bool;

} // namespace example
