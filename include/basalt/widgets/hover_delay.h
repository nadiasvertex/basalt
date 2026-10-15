#pragma once

#include <chrono>

namespace basalt {

// A countdown that a widget drives from `on_animation_frame`, for tooltips and
// hover popovers. `start` arms it and the widget requests a frame. Each frame
// passes the elapsed time to `advance`, which returns true once when the delay
// has run out. The widget asks for another frame while `pending` is true.
class hover_delay {
public:
  static constexpr std::chrono::milliseconds default_delay{500};

  explicit constexpr hover_delay(std::chrono::nanoseconds delay = default_delay) : delay_(delay) {}

  [[nodiscard]] constexpr auto delay() const -> std::chrono::nanoseconds { return delay_; }
  [[nodiscard]] constexpr auto pending() const -> bool { return pending_; }

  // Restarts the countdown.
  constexpr auto start() -> void {
    pending_ = true;
    waited_ = {};
  }
  constexpr auto cancel() -> void { pending_ = false; }

  // True once, on the frame where the waited time reaches the delay.
  [[nodiscard]] constexpr auto advance(std::chrono::nanoseconds elapsed) -> bool {
    if (!pending_) {
      return false;
    }
    waited_ += elapsed;
    if (waited_ < delay_) {
      return false;
    }
    pending_ = false;
    return true;
  }

private:
  std::chrono::nanoseconds delay_;
  std::chrono::nanoseconds waited_{};
  bool pending_ = false;
};

} // namespace basalt
