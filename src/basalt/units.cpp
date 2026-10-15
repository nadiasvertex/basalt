#include "basalt/units.h"

#include <algorithm>
#include <cmath>

namespace basalt {
namespace {

auto positive(float value) -> bool { return std::isfinite(value) && value > 0; }

} // namespace

auto is_valid(scale_factors factors) -> bool {
  return positive(factors.display) && positive(factors.zoom) && positive(factors.text);
}

auto snap_to_device(float layout_pixels, scale_factors factors) -> float {
  return from_device(std::round(to_device(layout_pixels, factors)), factors);
}

auto snap_to_device(rect r, scale_factors factors) -> rect {
  return rect::from_edges(snap_to_device(r.left(), factors), snap_to_device(r.top(), factors),
                          snap_to_device(r.right(), factors), snap_to_device(r.bottom(), factors));
}

auto device_stroke_width(length l, scale_factors factors) -> float {
  float const device = std::max(1.0f, std::round(to_device(to_layout(l, factors), factors)));
  return from_device(device, factors);
}

} // namespace basalt
