#pragma once

#include "basalt/geometry.h"
#include "basalt/paint.h"
#include "basalt/widget_tree.h"

namespace basalt {

// Records the paint commands for `tree` into `commands`, in root coordinates.
//
// Widgets paint back to front: a widget's `on_paint`, then its children in
// paint order, then its `on_paint_overlay`. Each widget is clipped to its
// bounds and drawn in its own coordinates. Hidden widgets are skipped, the
// top of a disabled subtree is drawn through a layer at the theme's disabled
// opacity, and subtrees outside `area` are not visited. The visible focus
// ring, if any, goes on top.
auto paint_tree(widget_tree const &tree, paint_commands &commands, rect area) -> void;

// The whole viewport.
auto paint_tree(widget_tree const &tree, paint_commands &commands) -> void;

} // namespace basalt
