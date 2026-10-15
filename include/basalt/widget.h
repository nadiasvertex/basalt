#pragma once

#include "basalt/accessibility/node.h"
#include "basalt/event.h"
#include "basalt/geometry.h"
#include "basalt/interaction.h"
#include "basalt/layout.h"
#include "basalt/units.h"
#include "basalt/widget_id.h"

#include <array>
#include <chrono>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <utility>
#include <vector>

namespace basalt {

class event_context;
class event_router;
class focus_manager;
class render_context;
struct theme;
class widget_tree;

enum class widget_state : std::uint8_t {
  // Not in a tree.
  detached,
  // Registered in a tree; `on_attached` has not run yet.
  attaching,
  attached,
  // Being removed; `on_detached` is pending or running.
  detaching,
};

// A node of the retained widget tree.
//
// Ownership: a parent owns its children and a `widget_tree` owns the root, so
// cycles and shared ownership are impossible. Everything else refers to
// widgets by `widget_id` or by short-lived references.
//
// Geometry: `bounds()` is in the parent's layout-pixel coordinates. Children
// are clipped to their parent's bounds, so damaging a widget's bounds covers
// its subtree and damage outside an ancestor is dropped.
//
// Measurement: `measure()` reports the size a widget wants within some
// constraints. Containers call it on their children from their own
// `on_measure` and `on_layout`; it must not change the tree. The last answers
// are remembered per constraints and dropped when the widget or a descendant
// invalidates layout, so `on_measure` runs once per change however many times
// a parent asks.
//
// Layout: `invalidate_layout()` marks the widget and every ancestor; the next
// `widget_tree::update_layout()` calls `on_layout()` top-down on each marked
// widget, which positions its children with `set_bounds`. Resizing a widget
// also marks it, so children of a resized widget are laid out in the same pass.
//
// Lifecycle: `on_attached` runs parent-first once a subtree joins a tree;
// `on_detached` runs children-first while the subtree is still registered.
// Hooks may add and remove other widgets, but widgets that are being detached
// cannot be removed again until detaching finishes.
//
// Painting: `on_paint` records the widget's own drawing and `paint_tree`
// (see basalt/widget_painter.h) recurses into the children and then calls
// `on_paint_overlay`. Widgets read their appearance from `theme()`.
//
// Input: an `event_router` delivers events to `on_event`, starting at the
// target and bubbling to the root until a widget returns true. Disabled
// widgets receive no events and are never hovered or pressed; Basalt keeps
// them focusable. `interaction()` holds the hover, press and focus flags.
class widget {
public:
  widget();
  virtual ~widget();

  widget(widget const &) = delete;
  auto operator=(widget const &) -> widget & = delete;

  [[nodiscard]] auto id() const -> widget_id { return id_; }
  [[nodiscard]] auto state() const -> widget_state { return state_; }
  [[nodiscard]] auto is_attached() const -> bool { return tree_ != nullptr; }
  // Null while detached.
  [[nodiscard]] auto tree() const -> widget_tree * { return tree_; }
  // Null for a tree root and for the top of a detached subtree.
  [[nodiscard]] auto parent() const -> widget * { return parent_; }
  [[nodiscard]] auto is_ancestor_of(widget const &other) const -> bool;
  // The nearest strict ancestor that is a `T`, or null.
  template <std::derived_from<widget> T> [[nodiscard]] auto find_ancestor() const -> T * {
    for (widget *w = parent_; w != nullptr; w = w->parent_) {
      if (auto *const match = dynamic_cast<T *>(w)) {
        return match;
      }
    }
    return nullptr;
  }
  // The parent when it is a `T`, or null.
  template <std::derived_from<widget> T> [[nodiscard]] auto parent_as() const -> T * {
    return dynamic_cast<T *>(parent_);
  }

  [[nodiscard]] auto child_count() const -> std::size_t { return children_.size(); }
  // Precondition: `index < child_count()`.
  [[nodiscard]] auto child(std::size_t index) const -> widget & { return *children_[index]; }
  [[nodiscard]] auto index_of(widget const &child) const -> std::optional<std::size_t>;
  // A view of the children in paint order (back to front). Invalidated by
  // changes to the child list.
  [[nodiscard]] auto children() const {
    return children_ | std::views::transform([](auto const &c) -> widget & { return *c; });
  }

  // Appends `child` and returns it. Throws `std::invalid_argument` when
  // `child` is null.
  template <std::derived_from<widget> T> auto add_child(std::unique_ptr<T> child) -> T & {
    return insert_child(children_.size(), std::move(child));
  }

  // Inserts `child` before position `index` (clamped to `child_count()`).
  // Throws `std::invalid_argument` when `child` is null.
  template <std::derived_from<widget> T>
  auto insert_child(std::size_t index, std::unique_ptr<T> child) -> T & {
    T *const added = child.get();
    insert_owned(index, std::unique_ptr<widget>(std::move(child)));
    return *added;
  }

  template <std::derived_from<widget> T, class... Args> auto emplace_child(Args &&...args) -> T & {
    return add_child(std::make_unique<T>(std::forward<Args>(args)...));
  }

  // Detaches `child` and hands back ownership. Null when `child` is not a
  // child of this widget or is already being detached.
  [[nodiscard]] auto remove_child(widget &child) -> std::unique_ptr<widget>;
  // `parent()->remove_child(*this)`; null for roots.
  [[nodiscard]] auto remove_from_parent() -> std::unique_ptr<widget>;
  // Removes every child. In a tree the children are destroyed later, so this is safe to call
  // from a child's own event handler.
  auto clear_children() -> void;
  // Moves `child` to position `index` (clamped). False when not a child.
  auto move_child(widget &child, std::size_t index) -> bool;

  [[nodiscard]] auto bounds() const -> rect { return bounds_; }
  // Called by the parent's layout. Damages the old and new areas and, when
  // the extent changes, requests layout of this widget.
  auto set_bounds(rect bounds) -> void;
  // Bounds in the root widget's coordinates.
  [[nodiscard]] auto bounds_in_root() const -> rect;
  [[nodiscard]] auto to_root(point local) const -> point;
  [[nodiscard]] auto from_root(point root) const -> point;

  // The size this widget wants within `limits`: `on_measure`'s answer clamped
  // to `limits`. Answers are remembered until layout is invalidated.
  [[nodiscard]] auto measure(constraints const &limits) const -> basalt::size;

  // The tree's theme, or one default theme while detached.
  [[nodiscard]] auto theme() const -> basalt::theme const &;
  // The tree's scale factors, or the defaults while detached.
  [[nodiscard]] auto scale() const -> scale_factors;
  // `l` in layout pixels under `scale()`.
  [[nodiscard]] auto resolve(length l) const -> float;
  [[nodiscard]] auto resolve(length_insets const &edges) const -> insets;

  [[nodiscard]] auto visible() const -> bool { return visible_; }
  // True when this widget and all its ancestors are visible.
  [[nodiscard]] auto visible_in_tree() const -> bool;
  auto set_visible(bool visible) -> void;

  // Disabling a widget disables its subtree.
  [[nodiscard]] auto enabled() const -> bool { return enabled_; }
  // True when this widget and all its ancestors are enabled.
  [[nodiscard]] auto enabled_in_tree() const -> bool;
  auto set_enabled(bool enabled) -> void;

  // A widget that is not hit-testable is never a pointer target itself, but
  // its children still are, so overlay layers can let the pointer through.
  [[nodiscard]] auto hit_testable() const -> bool { return hit_testable_; }
  auto set_hit_testable(bool hit_testable) -> void { hit_testable_ = hit_testable; }
  // Interactive controls set this so that their hit area grows to the
  // router's minimum target size when they are drawn smaller.
  [[nodiscard]] auto has_minimum_target() const -> bool { return minimum_target_; }
  auto set_minimum_target(bool enforce) -> void { minimum_target_ = enforce; }
  // Whether `local` (in this widget's coordinates and within its bounds)
  // hits the widget. The default accepts the whole bounds.
  [[nodiscard]] virtual auto contains_point(point local) const -> bool;

  [[nodiscard]] auto interaction() const -> interaction_state const & { return interaction_; }

  [[nodiscard]] auto focus_policy() const -> basalt::focus_policy { return focus_policy_; }
  // Losing every focus policy does not take focus away from a focused widget.
  auto set_focus_policy(basalt::focus_policy policy) -> void { focus_policy_ = policy; }
  [[nodiscard]] auto focus_scope() const -> basalt::focus_scope { return focus_scope_; }
  auto set_focus_scope(basalt::focus_scope scope) -> void { focus_scope_ = scope; }
  // True when attached, visible in the tree and the policy is not `none`.
  [[nodiscard]] auto accepts_focus() const -> bool;
  [[nodiscard]] auto has_focus() const -> bool { return interaction_.focused; }
  // `tree()->focus().set_focus(*this, reason)`; false while detached.
  auto request_focus(focus_reason reason = focus_reason::programmatic) -> bool;
  // Where the focus ring goes when this widget shows visible focus; null for
  // widgets that indicate focus themselves. The default rings the bounds
  // with the theme's default corner radius.
  [[nodiscard]] virtual auto focus_ring() const -> std::optional<focus_ring_shape>;

  // What this widget is and does for assistive technology. The default has
  // no role, which leaves the widget out of the semantic tree. Must not
  // change the tree.
  [[nodiscard]] virtual auto accessibility_info() const -> accessibility::info { return {}; }
  // Performs an action requested by an assistive technology. Focus is handled
  // here, other actions need the widget to be enabled and to list the action
  // in `accessibility_info().actions`, and they reach `on_accessibility_action`.
  // False when the widget refuses.
  auto perform_accessibility_action(accessibility::action_request const &request) -> bool;

  // Records this widget's own appearance and then, after its children,
  // its overlay. Both draw in this widget's coordinates, clipped to its
  // bounds. `paint_tree` calls them; children are not painted here.
  auto paint(render_context &context) const -> void;
  auto paint_overlay(render_context &context) const -> void;

  // Marks this widget and all ancestors for layout, since a change in this
  // widget's content may change how its parent arranges it.
  auto invalidate_layout() -> void;
  // Damages the whole widget, or `local` (in this widget's coordinates).
  // Ignored while detached or hidden.
  auto invalidate_paint() -> void;
  auto invalidate_paint(rect local) -> void;
  // `invalidate_layout()` and `invalidate_paint()`, for a change in content.
  auto invalidate_content() -> void;
  [[nodiscard]] auto needs_layout() const -> bool { return needs_layout_; }
  // True when this widget or a descendant needs layout.
  [[nodiscard]] auto subtree_needs_layout() const -> bool {
    return needs_layout_ || descendant_needs_layout_;
  }

protected:
  // Handles `e`, whose positions are in this widget's coordinates. Returns
  // true to stop the event from bubbling to the parent.
  virtual auto on_event(event_context &context, event const &e) -> bool {
    (void)context;
    (void)e;
    return false;
  }
  // `interaction()` changed from `previous`. The default repaints.
  virtual auto on_interaction_changed(interaction_state const &previous) -> void {
    (void)previous;
    invalidate_paint();
  }
  // Carries out `request`, which has passed the checks described at
  // `perform_accessibility_action`. Returns true when it was performed.
  virtual auto on_accessibility_action(accessibility::action_request const &request) -> bool {
    (void)request;
    return false;
  }
  // Draws the widget before its children. The default draws nothing.
  virtual auto on_paint(render_context &context) const -> void { (void)context; }
  // Draws over the children, for scrollbars and similar adornments.
  virtual auto on_paint_overlay(render_context &context) const -> void { (void)context; }
  virtual auto on_attached() -> void {}
  virtual auto on_detached() -> void {}
  // Arranges children with `set_bounds`. The default leaves them in place.
  virtual auto on_layout() -> void {}
  // The size wanted within `limits`; the result is clamped by `measure()`.
  // The default wants nothing beyond `limits.min`.
  [[nodiscard]] virtual auto on_measure(constraints const &limits) const -> basalt::size {
    return limits.min;
  }
  // `child` was removed and is already detached; layout is invalidated.
  virtual auto on_child_removed(widget &child) -> void { (void)child; }
  // The tree's theme or scale factors changed; layout and paint are already
  // invalidated.
  virtual auto on_environment_changed() -> void {}
  // The part of this widget that an ancestor scroll view shows moved. Content
  // that places children by what is visible can invalidate layout here, which
  // lets the placement happen in the frame that scrolled. The default does nothing.
  virtual auto on_viewport_moved() -> void {}
  // Asks for `on_animation_frame` on the next frame. Ignored while detached.
  auto request_animation_frame() -> void;
  // Runs once per frame after `request_animation_frame`, with the time since the
  // previous animation frame. Returns true to run again on the next frame.
  virtual auto on_animation_frame(std::chrono::nanoseconds elapsed) -> bool {
    (void)elapsed;
    return false;
  }

private:
  friend class event_router;
  friend class focus_manager;
  friend class widget_tree;
  friend class scroll_view;

  struct measured_size {
    constraints limits;
    basalt::size result;
  };

  auto drop_measurements() const -> void;
  auto set_interaction_flag(bool interaction_state::*flag, bool value) -> void;
  auto insert_owned(std::size_t index, std::unique_ptr<widget> child) -> void;
  auto mark_needs_layout() -> void;
  auto attach_subtree(widget_tree &tree) -> void;
  auto detach_subtree() -> void;
  auto for_each_in_subtree(std::function<void(widget &)> const &visit) -> void;

  widget_id id_;
  widget_state state_ = widget_state::detached;
  widget_tree *tree_ = nullptr;
  widget *parent_ = nullptr;
  std::vector<std::unique_ptr<widget>> children_;
  rect bounds_;
  interaction_state interaction_;
  basalt::focus_policy focus_policy_ = focus_policy::none;
  basalt::focus_scope focus_scope_ = focus_scope::none;
  bool visible_ = true;
  bool enabled_ = true;
  bool hit_testable_ = true;
  bool minimum_target_ = false;
  bool needs_layout_ = true;
  bool descendant_needs_layout_ = false;
  mutable std::array<std::optional<measured_size>, 2> measured_;
  mutable std::size_t measured_next_ = 0;
};

} // namespace basalt
