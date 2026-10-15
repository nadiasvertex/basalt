#include "basalt/layout.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace basalt {

namespace {

// Below this the remaining free space is rounding noise.
constexpr float free_space_epsilon = 1e-3f;

} // namespace

auto distribute(std::span<linear_item const> items, float available) -> std::vector<float> {
  std::vector<float> sizes;
  std::vector<bool> frozen(items.size(), false);
  sizes.reserve(items.size());
  for (auto const &item : items) {
    sizes.push_back(std::clamp(item.basis, item.min, std::max(item.min, item.max)));
  }

  // Every round either finishes or freezes at least one item, so this ends.
  for (std::size_t round = 0; round <= items.size(); ++round) {
    float used = 0;
    for (auto const s : sizes) {
      used += s;
    }
    float const free = available - used;
    if (std::abs(free) < free_space_epsilon || !std::isfinite(free)) {
      break;
    }
    bool const growing = free > 0;

    auto weight = [&](std::size_t i) -> float {
      if (frozen[i]) {
        return 0;
      }
      return growing ? items[i].grow : items[i].shrink * sizes[i];
    };
    float total = 0;
    for (std::size_t i = 0; i < items.size(); ++i) {
      total += weight(i);
    }
    if (total <= 0) {
      break;
    }

    bool any_clamped = false;
    std::vector<float> proposed(sizes);
    for (std::size_t i = 0; i < items.size(); ++i) {
      float const w = weight(i);
      if (w <= 0) {
        continue;
      }
      float const wanted = sizes[i] + free * w / total;
      float const limited = std::clamp(wanted, items[i].min, std::max(items[i].min, items[i].max));
      if (limited != wanted) {
        any_clamped = true;
        frozen[i] = true;
      }
      proposed[i] = limited;
    }
    if (!any_clamped) {
      sizes = proposed;
      break;
    }
    // Commit only the clamped items and share what is left among the others.
    for (std::size_t i = 0; i < items.size(); ++i) {
      if (frozen[i]) {
        sizes[i] = proposed[i];
      }
    }
  }
  return sizes;
}

} // namespace basalt
