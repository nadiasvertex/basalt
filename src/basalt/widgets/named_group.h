#pragma once

#include "basalt/accessibility/node.h"

#include <string>

namespace basalt {

// What assistive technology is told about a named group of controls.
[[nodiscard]] inline auto named_group_info(std::string const &name) -> accessibility::info {
  accessibility::info details;
  details.kind = accessibility::role::group;
  details.name = name;
  return details;
}

} // namespace basalt
