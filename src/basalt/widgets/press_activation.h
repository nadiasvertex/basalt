#pragma once

#include "basalt/event.h"
#include "basalt/input.h"
#include "basalt/interaction.h"

#include <functional>
#include <type_traits>
#include <variant>

namespace basalt {

// Keys that activate a press target beyond the always-accepted Space.
struct press_keys {
  bool enter = true;
  bool down = false;
};

// Implements the activation rules of a press target. A primary press is
// consumed, a primary release activates when the widget is still pressed, and
// an unmodified, non-repeating key press of Space, or of a key enabled in
// `keys`, activates. Returns whether the event was consumed.
//
// `activate` may return `bool`, which then decides whether a key press is
// consumed. Pointer events are always consumed once they are a primary press
// or release.
template <class Activate>
auto handle_press_activation(event const &e, interaction_state const &state, press_keys keys,
                             Activate &&activate) -> bool {
  auto const run = [&]() -> bool {
    if constexpr (std::is_void_v<std::invoke_result_t<Activate &>>) {
      activate();
      return true;
    } else {
      return static_cast<bool>(activate());
    }
  };

  if (auto const *p = std::get_if<pointer_event>(&e)) {
    if (p->button != pointer_button::primary) {
      return false;
    }
    if (p->action == pointer_action::down) {
      return true;
    }
    if (p->action == pointer_action::up) {
      if (state.pressed) {
        activate();
      }
      return true;
    }
    return false;
  }
  if (auto const *k = std::get_if<key_event>(&e)) {
    if (k->pressed && !k->repeat && k->modifiers == modifier::none &&
        (k->code == key::space || (k->code == key::enter && keys.enter) ||
         (k->code == key::down && keys.down))) {
      return run();
    }
  }
  return false;
}

} // namespace basalt
