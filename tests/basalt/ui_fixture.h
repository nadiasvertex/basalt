#pragma once

#include "basalt/accessibility/tree.h"
#include "basalt/event_router.h"
#include "basalt/widget_painter.h"
#include "basalt/widget_tree.h"

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include <variant>

#include "input_probe.h"

namespace basalt::testing {

// A window-sized tree with an event router, laid out after every change.
struct ui {
  explicit ui(basalt::size viewport = {400, 300}, scale_factors scale = {}) : tree({}, scale) {
    tree.set_viewport(viewport);
  }

  auto layout() -> void { tree.update_layout(); }

  auto send(event const &e) -> bool {
    bool const handled = router.dispatch(e);
    layout();
    return handled;
  }
  auto click(point at) -> void {
    send(press(at));
    send(release(at));
  }
  auto tap(key code, modifier modifiers = modifier::none) -> bool {
    return send(key_press(code, modifiers));
  }
  auto type(std::string text) -> bool { return send(text_input_event{std::move(text)}); }

  // Runs animation frames 60 times a second for `duration` of simulated time.
  auto advance(std::chrono::milliseconds duration) -> void {
    constexpr std::chrono::nanoseconds frame{16'666'667};
    for (std::chrono::nanoseconds spent{}; spent < duration; spent += frame) {
      now += frame;
      tree.advance_animations(now);
      layout();
    }
  }

  [[nodiscard]] auto commands() -> paint_commands {
    paint_commands out;
    paint_tree(tree, out);
    return out;
  }
  [[nodiscard]] auto semantics() const -> accessibility::tree { return accessibility::build(tree); }

  widget_tree tree;
  event_router router{tree};
  std::chrono::nanoseconds now{};
};

template <class T> auto count_commands(paint_commands const &commands) -> int {
  return static_cast<int>(std::ranges::count_if(
      commands, [](paint_command const &c) { return std::holds_alternative<T>(c); }));
}

} // namespace basalt::testing
