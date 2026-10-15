#pragma once

#include "basalt/color.h"
#include "basalt/geometry.h"
#include "basalt/interaction.h"
#include "basalt/widget.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>
#include <vector>

namespace basalt {

class widget_tree;

enum class focus_move : std::uint8_t { next, previous, first, last };

// Keyboard focus for one widget tree, which owns it.
//
// Traversal: Tab stops are the widgets whose policy allows Tab, in tree order
// (parents before children, children in paint order), skipping hidden
// subtrees. Disabled widgets stay Tab stops, as Cubit requires. A
// `focus_scope::group` widget contributes one stop: the descendant focused
// last if it is still a Tab stop, otherwise its first one. `next` and
// `previous` wrap around.
//
// Modals: `push_modal` traps focus and pointer input inside a widget until
// `pop_modal`, which restores the focus held before the push. Modals nest;
// the most recent one is active. A modal that leaves the tree is popped.
//
// Focus visibility follows the input modality, as CSS `:focus-visible` does:
// focus shows a ring after keyboard navigation or a key press, and not after
// a pointer press. Programmatic focus keeps the current modality. Focus is
// never visible while the window is inactive.
//
// When the focused widget leaves the tree or is hidden, focus moves to its
// nearest ancestor that accepts focus, then to the active modal's first Tab
// stop, and is otherwise cleared.
class focus_manager {
public:
  explicit focus_manager(widget_tree &tree) : tree_(&tree) {}

  focus_manager(focus_manager const &) = delete;
  auto operator=(focus_manager const &) -> focus_manager & = delete;

  // Null when nothing has focus.
  [[nodiscard]] auto focused() const -> widget *;

  // Focuses `w`. False when `w` does not accept focus, belongs to another
  // tree, or is outside the active modal.
  auto set_focus(widget &w, focus_reason reason = focus_reason::programmatic) -> bool;
  auto clear_focus() -> void;
  // Focuses the nearest ancestor-or-self of `target` whose policy allows
  // focus by click. Leaves focus unchanged when there is none, so pressing
  // on inert content does not take focus from an editor. Returns true when
  // focus was set.
  auto focus_from_pointer(widget &target) -> bool;

  // Moves among the Tab stops of the active modal, or of the whole tree.
  auto move_focus(focus_move move, focus_reason reason = focus_reason::keyboard) -> bool;
  // Moves among the stops of the innermost group scope containing the
  // focused widget, as arrow keys do in a toolbar. False when focus is not in
  // a group. `wrap` applies to `next` and `previous`.
  auto move_focus_in_group(focus_move move, bool wrap = true,
                           focus_reason reason = focus_reason::keyboard) -> bool;
  // The widgets Tab visits within `within` (default: the active modal or
  // the root), in order.
  [[nodiscard]] auto tab_order(widget const *within = nullptr) const -> std::vector<widget *>;
  // Makes `entry` the stop that Tab enters `group` at. False when `group`
  // is not a group scope or `entry` is not inside it.
  auto set_group_entry(widget &group, widget &entry) -> bool;

  // Activates `modal` and moves focus into it unless it already holds focus:
  // to its first Tab stop, else to the modal itself when it accepts focus,
  // else nowhere. False when `modal` is not attached to this tree.
  auto push_modal(widget &modal) -> bool;
  // Deactivates `modal`. When it was the active modal, focus returns to the
  // widget focused before the push if it can still take focus. False when
  // `modal` was not pushed.
  auto pop_modal(widget &modal) -> bool;
  // The active modal, or null.
  [[nodiscard]] auto modal_root() const -> widget *;
  // True when a modal is active and `w` is outside it.
  [[nodiscard]] auto is_blocked(widget const &w) const -> bool;

  [[nodiscard]] auto keyboard_modality() const -> bool { return keyboard_modality_; }
  // The event router calls this on key presses (true) and pointer presses
  // (false).
  auto set_keyboard_modality(bool keyboard) -> void;
  [[nodiscard]] auto window_active() const -> bool { return window_active_; }
  auto set_window_active(bool active) -> void;
  [[nodiscard]] auto focus_visible() const -> bool;

  // Called after every change of the focused widget with the new one (or
  // null), for example to start or stop platform text input.
  auto set_focus_changed_handler(std::function<void(widget *)> handler) -> void;

  // Kept by `widget` and `widget_tree`.
  auto subtree_detached(widget *former_parent) -> void;
  auto subtree_hidden(widget &top) -> void;
  // Forgets all state without notifying anyone; used when the tree is torn
  // down.
  auto reset() -> void;

private:
  friend class widget;

  struct modal_entry {
    widget_id modal;
    widget_id restore;
  };

  auto change_focus(widget *next, focus_reason reason) -> void;
  auto refresh_flags() -> void;
  auto damage_ring(widget const &w) -> void;
  auto remember_in_groups(widget &w) -> void;
  auto fall_back(widget *from) -> void;
  auto move_among(widget &root, focus_move move, bool wrap, focus_reason reason) -> bool;
  [[nodiscard]] auto traversal_root() const -> widget *;
  [[nodiscard]] auto group_entry(widget &group) const -> widget *;

  widget_tree *tree_;
  widget_id focused_;
  // `focused_` and its ancestors as of the last change, so their flags can be
  // cleared even after `focused_` has left the tree.
  std::vector<widget_id> chain_;
  std::vector<modal_entry> modals_;
  std::unordered_map<widget_id, widget_id> group_entries_;
  std::function<void(widget *)> changed_handler_;
  bool keyboard_modality_ = false;
  bool window_active_ = true;
};

// A focus ring ready to draw, in root coordinates.
struct focus_ring {
  // The outer edge of the ring.
  rect outer;
  // Stroke width; the ring covers `outer` minus `outer` inset by `width`.
  float width = 0;
  // Corner radius of `outer`.
  float corner_radius = 0;
  rgba color;
  // The visible part of the focused widget's parent chain; the ring is
  // clipped to it.
  rect clip;

  [[nodiscard]] friend constexpr auto operator==(focus_ring const &, focus_ring const &)
      -> bool = default;
};

// The Cubit focus ring around `shape` (in `w`'s coordinates):
// `size.dimension.focus-ring` wide in `color.border.focus`, outside the
// shape by `size.gap.focus-ring`, snapped to device pixels.
[[nodiscard]] auto make_focus_ring(widget const &w, focus_ring_shape const &shape) -> focus_ring;

// The ring to draw this frame, if focus is visible and the focused widget
// wants one.
[[nodiscard]] auto visible_focus_ring(widget_tree const &tree) -> std::optional<focus_ring>;

} // namespace basalt
