#pragma once

#include <cstddef>

namespace basalt {

// Item indices from `first` up to, but not including, `last`.
struct index_range {
  std::size_t first = 0;
  std::size_t last = 0;

  [[nodiscard]] constexpr auto empty() const -> bool { return last <= first; }
  [[nodiscard]] constexpr auto size() const -> std::size_t { return empty() ? 0 : last - first; }
  [[nodiscard]] constexpr auto contains(std::size_t index) const -> bool {
    return index >= first && index < last;
  }

  [[nodiscard]] friend constexpr auto operator==(index_range const &, index_range const &)
      -> bool = default;
};

} // namespace basalt
