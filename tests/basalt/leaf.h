#pragma once

#include "basalt/layout.h"
#include "basalt/widget.h"

namespace basalt::testing {

// A leaf that wants a fixed size and counts its layouts.
struct leaf : widget {
  explicit leaf(basalt::size wanted = {}) : wanted(wanted) {}

  auto set_wanted(basalt::size s) -> void {
    wanted = s;
    invalidate_layout();
  }

  basalt::size wanted;
  int layouts = 0;

protected:
  [[nodiscard]] auto on_measure(constraints const &) const -> basalt::size override {
    return wanted;
  }
  auto on_layout() -> void override { ++layouts; }
};

} // namespace basalt::testing
