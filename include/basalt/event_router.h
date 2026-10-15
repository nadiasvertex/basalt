#pragma once

#include "basalt/event.h"
#include "basalt/geometry.h"
#include "basalt/units.h"
#include "basalt/widget.h"

#include <optional>
#include <vector>

namespace basalt {

class event_router;
class widget_tree;

// Passed to `widget::on_event` for one delivery.
class event_context {
public:
  event_context(event_context const &) = delete;
  auto operator=(event_context const &) -> event_context & = delete;

  // The widget the event was aimed at, which may be a descendant of
  // `current()`; null once a handler has removed it from the tree.
  [[nodiscard]] auto target() const -> widget *;
  // The widget receiving the event now.
  [[nodiscard]] auto current() const -> widget & { return *current_; }
  // The pointer position in root coordinates, for pointer and wheel events.
  [[nodiscard]] auto root_position() const -> std::optional<point> { return root_position_; }
  [[nodiscard]] auto router() const -> event_router & { return *router_; }

  // Sends the rest of this pointer interaction to `current()`, wherever the
  // pointer goes, until every button is released. A primary press that a
  // widget handles captures the pointer for it automatically.
  auto capture_pointer() -> void;
  // Ends capture, and keeps a handled press from capturing.
  auto release_pointer() -> void;

private:
  friend class event_router;

  event_context(event_router &router, widget_id target, std::optional<point> root_position)
      : router_(&router), target_(target), root_position_(root_position) {}

  event_router *router_;
  widget_id target_;
  widget *current_ = nullptr;
  std::optional<point> root_position_;
};

// Turns normalized platform events into widget deliveries for one tree.
//
// Pointer events go to the widget under the pointer, or to the widget that
// captured the pointer, and bubble to the root. File drops go to the widget
// under the drop position; the widget that last accepted an `enter` or `move`
// is told when the drag leaves it. Keyboard, text, IME and
// command events go to the focused widget (or, with nothing focused, to the
// active modal or the root) and bubble up to the active modal or the root.
// Positions are converted to each receiving widget's coordinates. Disabled
// widgets are skipped.
//
// Hit testing visits children front to back and clips them to their parent.
// A widget with `has_minimum_target()` is also hit within its bounds grown to
// `minimum_target()` around its centre. A hit within a widget's bounds wins
// over a hit in grown areas; among grown areas the nearest bounds win, so
// neighbouring small controls split the space between them.
//
// The router keeps `hovered` and `pressed` current; focus belongs to the
// tree's `focus_manager`. A pointer press focuses the innermost widget that
// accepts focus by click. An unhandled Tab or Shift+Tab moves focus. While a
// modal is active, pointer input outside it is dropped and a capture outside
// it is cancelled.
//
// The router refers to widgets by id and must not outlive its tree.
class event_router {
public:
  explicit event_router(widget_tree &tree) : tree_(&tree) {}

  event_router(event_router const &) = delete;
  auto operator=(event_router const &) -> event_router & = delete;

  // Returns true when a widget handled `e` or the router acted on it. Window
  // resize, scale and activation events update the tree; close, show and
  // hide requests are left to the application.
  auto dispatch(event const &e) -> bool;

  // Recomputes hover for the last pointer position. Call after layout, since
  // widgets may have moved under a still pointer.
  auto update_hover() -> void;

  // The widget that a pointer press at `root_position` would reach, or null.
  [[nodiscard]] auto hit_test(point root_position) const -> widget *;
  // `w`'s hit area in its own coordinates: its bounds, grown to the minimum
  // target size when `w` has a minimum target.
  [[nodiscard]] auto hit_area(widget const &w) const -> rect;

  [[nodiscard]] auto minimum_target() const -> length { return minimum_target_; }
  auto set_minimum_target(length target) -> void { minimum_target_ = target; }

  // Null when there is none.
  [[nodiscard]] auto hovered() const -> widget *;
  [[nodiscard]] auto pressed() const -> widget *;
  [[nodiscard]] auto pointer_capture() const -> widget *;
  // Captures for `w`, which must be in this tree and not blocked by a modal.
  auto capture_pointer(widget &w) -> bool;
  auto release_pointer() -> void;

  // Cubit compliance asks for 40 px targets even where the controls are drawn
  // at a denser size.
  static constexpr length default_minimum_target = fixed(40);

private:
  friend class event_context;

  auto dispatch_pointer(pointer_event const &e) -> bool;
  auto dispatch_wheel(wheel_event const &e) -> bool;
  auto dispatch_drop(file_drop_event const &e) -> bool;
  auto end_drop(event_time time) -> void;
  auto dispatch_focused(event const &e) -> bool;
  auto dispatch_window(window_event const &e) -> bool;
  // Delivers `e` from `target` up to the root (or `stop`, inclusive) and
  // returns the id of the widget that handled it, or an invalid id.
  auto deliver(widget &target, event const &e, widget const *stop = nullptr) -> widget_id;
  auto cancel_interaction(event_time time) -> void;
  auto set_hover(widget *leaf) -> void;
  auto refresh_pressed() -> void;
  // Sets `flag` on the widgets in `next` and clears it on the rest of `chain`.
  auto update_chain(std::vector<widget_id> &chain, std::vector<widget_id> next,
                    bool interaction_state::*flag) -> void;
  [[nodiscard]] auto pick(widget &w, point local) const -> widget *;

  widget_tree *tree_;
  length minimum_target_ = default_minimum_target;
  widget_id capture_;
  widget_id press_target_;
  widget_id hover_target_;
  std::vector<widget_id> hover_chain_;
  std::vector<widget_id> pressed_chain_;
  std::optional<point> pointer_position_;
  widget_id drop_target_;
  point drop_position_;
  // Set by `event_context::release_pointer` during the current delivery.
  bool capture_declined_ = false;
};

} // namespace basalt
