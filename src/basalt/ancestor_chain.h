#pragma once

#include "basalt/widget.h"
#include "basalt/widget_id.h"

#include <vector>

namespace basalt {

// The ids from `leaf` up to the root, or up to and including `stop` when it is on the path.
[[nodiscard]] inline auto ancestor_chain(widget const *leaf, widget const *stop = nullptr)
    -> std::vector<widget_id> {
  auto chain = std::vector<widget_id>{};
  for (auto const *w = leaf; w != nullptr; w = w->parent()) {
    chain.push_back(w->id());
    if (w == stop) {
      break;
    }
  }
  return chain;
}

} // namespace basalt
