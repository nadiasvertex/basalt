#pragma once

#include <utility>

namespace basalt {

// Calls `handler` with `args` when it is set. The handler runs from a copy
// because it may destroy the widget that owns it; callers must not touch their
// members afterwards, and must pass copies of anything the handler could
// invalidate.
template <class Handler, class... Args>
auto notify(Handler const &handler, Args &&...args) -> void {
  if (handler) {
    auto const copy = handler;
    copy(std::forward<Args>(args)...);
  }
}

} // namespace basalt
