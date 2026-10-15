#include "sdl_events.h"

#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>

#include <chrono>
#include <string>

namespace example {
namespace {

auto translate_key(SDL_Keycode value) -> basalt::key {
  if (value >= SDLK_A && value <= SDLK_Z) {
    return static_cast<basalt::key>(static_cast<int>(basalt::key::a) + value - SDLK_A);
  }
  if (value >= SDLK_0 && value <= SDLK_9) {
    return static_cast<basalt::key>(static_cast<int>(basalt::key::digit_0) + value - SDLK_0);
  }
  if (value >= SDLK_F1 && value <= SDLK_F12) {
    return static_cast<basalt::key>(static_cast<int>(basalt::key::f1) + value - SDLK_F1);
  }
  switch (value) {
  case SDLK_LEFT:
    return basalt::key::left;
  case SDLK_RIGHT:
    return basalt::key::right;
  case SDLK_UP:
    return basalt::key::up;
  case SDLK_DOWN:
    return basalt::key::down;
  case SDLK_HOME:
    return basalt::key::home;
  case SDLK_END:
    return basalt::key::end;
  case SDLK_PAGEUP:
    return basalt::key::page_up;
  case SDLK_PAGEDOWN:
    return basalt::key::page_down;
  case SDLK_RETURN:
  case SDLK_KP_ENTER:
    return basalt::key::enter;
  case SDLK_ESCAPE:
    return basalt::key::escape;
  case SDLK_TAB:
    return basalt::key::tab;
  case SDLK_SPACE:
    return basalt::key::space;
  case SDLK_BACKSPACE:
    return basalt::key::backspace;
  case SDLK_DELETE:
    return basalt::key::delete_forward;
  case SDLK_INSERT:
    return basalt::key::insert;
  case SDLK_LSHIFT:
    return basalt::key::left_shift;
  case SDLK_RSHIFT:
    return basalt::key::right_shift;
  case SDLK_LCTRL:
    return basalt::key::left_control;
  case SDLK_RCTRL:
    return basalt::key::right_control;
  case SDLK_LALT:
    return basalt::key::left_alt;
  case SDLK_RALT:
    return basalt::key::right_alt;
  case SDLK_LGUI:
    return basalt::key::left_meta;
  case SDLK_RGUI:
    return basalt::key::right_meta;
  default:
    return basalt::key::unknown;
  }
}

auto translate_modifiers(SDL_Keymod value) -> basalt::modifier {
  auto result = basalt::modifier::none;
  if ((value & SDL_KMOD_SHIFT) != 0) {
    result |= basalt::modifier::shift;
  }
  if ((value & SDL_KMOD_CTRL) != 0) {
    result |= basalt::modifier::control;
  }
  if ((value & SDL_KMOD_ALT) != 0) {
    result |= basalt::modifier::alt;
  }
  if ((value & SDL_KMOD_GUI) != 0) {
    result |= basalt::modifier::meta;
  }
  return result;
}

auto translate_button(Uint8 value) -> basalt::pointer_button {
  switch (value) {
  case SDL_BUTTON_LEFT:
    return basalt::pointer_button::primary;
  case SDL_BUTTON_RIGHT:
    return basalt::pointer_button::secondary;
  case SDL_BUTTON_MIDDLE:
    return basalt::pointer_button::middle;
  default:
    return basalt::pointer_button::none;
  }
}

auto translate_buttons(SDL_MouseButtonFlags value) -> basalt::pointer_button {
  auto result = basalt::pointer_button::none;
  if ((value & SDL_BUTTON_LMASK) != 0) {
    result |= basalt::pointer_button::primary;
  }
  if ((value & SDL_BUTTON_RMASK) != 0) {
    result |= basalt::pointer_button::secondary;
  }
  if ((value & SDL_BUTTON_MMASK) != 0) {
    result |= basalt::pointer_button::middle;
  }
  return result;
}

// SDL timestamps are nanoseconds since SDL started.
auto timestamp(SDL_Event const &event) -> basalt::event_time {
  return std::chrono::nanoseconds{static_cast<std::int64_t>(event.common.timestamp)};
}

} // namespace

auto translate_event(SDL_Event const &event) -> std::optional<basalt::event> {
  switch (event.type) {
  case SDL_EVENT_KEY_DOWN:
  case SDL_EVENT_KEY_UP: {
    auto value = basalt::key_event{};
    value.code = translate_key(event.key.key);
    value.pressed = event.key.down;
    value.repeat = event.key.repeat;
    value.modifiers = translate_modifiers(event.key.mod);
    value.time = timestamp(event);
    return value;
  }
  case SDL_EVENT_TEXT_INPUT:
    return basalt::text_input_event{event.text.text != nullptr ? event.text.text : "",
                                    timestamp(event)};
  case SDL_EVENT_MOUSE_MOTION: {
    auto value = basalt::pointer_event{};
    value.action = basalt::pointer_action::move;
    value.position = {event.motion.x, event.motion.y};
    value.buttons = translate_buttons(event.motion.state);
    value.modifiers = translate_modifiers(SDL_GetModState());
    value.time = timestamp(event);
    return value;
  }
  case SDL_EVENT_MOUSE_BUTTON_DOWN:
  case SDL_EVENT_MOUSE_BUTTON_UP: {
    auto value = basalt::pointer_event{};
    value.action = event.button.down ? basalt::pointer_action::down : basalt::pointer_action::up;
    value.position = {event.button.x, event.button.y};
    value.button = translate_button(event.button.button);
    auto state = static_cast<SDL_MouseButtonFlags>(SDL_GetMouseState(nullptr, nullptr));
    if (event.button.down) {
      state |= SDL_BUTTON_MASK(event.button.button);
    } else {
      state &= ~SDL_BUTTON_MASK(event.button.button);
    }
    value.buttons = translate_buttons(state);
    value.modifiers = translate_modifiers(SDL_GetModState());
    value.click_count = event.button.clicks == 0 ? 1 : event.button.clicks;
    value.time = timestamp(event);
    return value;
  }
  case SDL_EVENT_MOUSE_WHEEL: {
    // SDL reports positive y as scrolling away from the user, which reveals content above;
    // Basalt's positive y reveals content below. A flipped direction means natural scrolling.
    auto const direction = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1.0F : 1.0F;
    auto value = basalt::wheel_event{};
    value.position = {event.wheel.mouse_x, event.wheel.mouse_y};
    value.delta = {event.wheel.x * direction * wheel_notch_layout_pixels,
                   -event.wheel.y * direction * wheel_notch_layout_pixels};
    value.precise = true;
    value.modifiers = translate_modifiers(SDL_GetModState());
    value.time = timestamp(event);
    return value;
  }
  case SDL_EVENT_WINDOW_MOUSE_LEAVE: {
    auto value = basalt::pointer_event{};
    value.action = basalt::pointer_action::leave;
    value.time = timestamp(event);
    return value;
  }
  case SDL_EVENT_WINDOW_FOCUS_GAINED:
  case SDL_EVENT_WINDOW_FOCUS_LOST: {
    auto value = basalt::window_event{};
    value.action = event.type == SDL_EVENT_WINDOW_FOCUS_GAINED ? basalt::window_action::focus_gained
                                                               : basalt::window_action::focus_lost;
    value.time = timestamp(event);
    return value;
  }
  default:
    return std::nullopt;
  }
}

} // namespace example
