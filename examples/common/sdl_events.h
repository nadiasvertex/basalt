#pragma once

#include "basalt/event.h"

#include <SDL3/SDL_events.h>

#include <optional>

namespace example {

// Layout pixels moved by one wheel notch.
inline constexpr float wheel_notch_layout_pixels = 40;

// Converts an SDL input event to a Basalt event. Window and quit events are not input and return
// nothing; the application handles them itself. SDL reports window coordinates, which are Basalt's
// layout pixels.
[[nodiscard]] auto translate_event(SDL_Event const &event) -> std::optional<basalt::event>;

} // namespace example
