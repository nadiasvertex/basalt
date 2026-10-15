#pragma once

#include "basalt/geometry.h"
#include "basalt/widget.h"
#include "basalt/widgets/scroll_view.h"

namespace basalt {

// Scrolls the nearest enclosing scroll view so that `area`, in `w`'s
// coordinates, is visible. Does nothing when `w` is not inside one.
inline auto scroll_into_view_of(widget const &w, rect area) -> void {
  auto *const view = w.find_ancestor<scroll_view>();
  if (view == nullptr) {
    return;
  }
  if (widget *const content = view->content()) {
    view->scroll_into_view(translated(area, w.to_root({}) - content->to_root({})));
  }
}

} // namespace basalt
