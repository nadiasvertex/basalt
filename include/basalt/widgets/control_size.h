#pragma once

#include "basalt/widgets/text_line.h"

#include <cstdint>

namespace basalt {

// The component sizes of the Cubit specification. They follow the body text
// sizes, so a control's label uses the matching text role.
enum class control_size : std::uint8_t { small, medium, large };

[[nodiscard]] constexpr auto label_role_for(control_size size) -> text_role {
  switch (size) {
  case control_size::small:
    return text_role::small;
  case control_size::large:
    return text_role::large;
  case control_size::medium:
    break;
  }
  return text_role::body;
}

} // namespace basalt
