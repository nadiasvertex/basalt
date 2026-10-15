#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>

namespace basalt {

// Identifies a widget for its whole lifetime. Ids are unique within the
// process and never reused, and they survive reparenting, so focus, pointer
// capture and accessibility can refer to a widget without owning it and
// resolve it later with `widget_tree::find`.
struct widget_id {
  std::uint64_t value = 0;

  [[nodiscard]] constexpr auto valid() const -> bool { return value != 0; }

  [[nodiscard]] friend constexpr auto operator==(widget_id, widget_id) -> bool = default;
  [[nodiscard]] friend constexpr auto operator<=>(widget_id, widget_id) = default;
};

} // namespace basalt

template <> struct std::hash<basalt::widget_id> {
  [[nodiscard]] auto operator()(basalt::widget_id id) const noexcept -> std::size_t {
    return std::hash<std::uint64_t>{}(id.value);
  }
};
