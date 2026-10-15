#pragma once

#include "basalt/widget.h"

#include <optional>

namespace basalt {

// A horizontal bar that shows how far an operation has come. A fraction from
// 0 to 1 fills the bar from the left. Without a fraction the bar is
// indeterminate and shows a short segment that moves along the track each time
// `tick` is called.
class progress_bar : public widget {
public:
  explicit progress_bar(std::optional<float> fraction = std::nullopt);

  [[nodiscard]] auto fraction() const -> std::optional<float> { return fraction_; }
  // Values outside 0 to 1 are clamped.
  auto set_fraction(std::optional<float> fraction) -> void;

  // Moves the indeterminate segment by `step` of the track width and repaints,
  // which asks for the next frame. A bar that has a fraction only repaints.
  // Call it once per frame while the operation runs.
  auto tick(float step) -> void;

  [[nodiscard]] auto accessibility_info() const -> accessibility::info override;

protected:
  auto on_paint(render_context &context) const -> void override;
  [[nodiscard]] auto on_measure(constraints const &limits) const -> basalt::size override;

private:
  std::optional<float> fraction_;
  float phase_ = 0;
};

} // namespace basalt
