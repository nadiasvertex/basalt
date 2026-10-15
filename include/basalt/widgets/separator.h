#pragma once

#include "basalt/layout.h"
#include "basalt/widget.h"

namespace basalt {

// A hairline between groups of controls. With `axis::horizontal` the line
// runs left to right and the separator stretches across its parent.
class separator : public widget {
public:
  explicit separator(axis direction = axis::horizontal) : direction_(direction) {}

  [[nodiscard]] auto direction() const -> axis { return direction_; }
  auto set_direction(axis direction) -> void;

  [[nodiscard]] auto accessibility_info() const -> accessibility::info override;

protected:
  auto on_paint(render_context &context) const -> void override;
  [[nodiscard]] auto on_measure(constraints const &limits) const -> basalt::size override;

private:
  axis direction_;
};

} // namespace basalt
