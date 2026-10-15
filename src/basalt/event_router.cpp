#include "basalt/event_router.h"

#include "basalt/widget_tree.h"

#include <algorithm>
#include <limits>
#include <ranges>
#include <utility>
#include <variant>
#include <vector>

namespace basalt {

namespace {

auto distance_squared(point p, rect r) -> float {
  float const dx = std::max({r.left() - p.x, 0.0f, p.x - r.right()});
  float const dy = std::max({r.top() - p.y, 0.0f, p.y - r.bottom()});
  return dx * dx + dy * dy;
}

auto with_position(event const &e, point position) -> event {
  event copy = e;
  if (auto *pointer = std::get_if<pointer_event>(&copy)) {
    pointer->position = position;
  } else if (auto *wheel = std::get_if<wheel_event>(&copy)) {
    wheel->position = position;
  } else if (auto *drop = std::get_if<file_drop_event>(&copy)) {
    drop->position = position;
  }
  return copy;
}

auto chain_of(widget *leaf) -> std::vector<widget_id> {
  std::vector<widget_id> chain;
  for (widget *w = leaf; w != nullptr; w = w->parent()) {
    chain.push_back(w->id());
  }
  return chain;
}

} // namespace

auto event_context::capture_pointer() -> void { router_->capture_pointer(*current_); }

auto event_context::release_pointer() -> void { router_->release_pointer(); }

auto event_context::target() const -> widget * { return router_->tree_->find(target_); }

auto event_router::dispatch(event const &e) -> bool {
  return std::visit(
      [&](auto const &value) -> bool {
        using type = std::remove_cvref_t<decltype(value)>;
        if constexpr (std::is_same_v<type, pointer_event>) {
          return dispatch_pointer(value);
        } else if constexpr (std::is_same_v<type, wheel_event>) {
          return dispatch_wheel(value);
        } else if constexpr (std::is_same_v<type, file_drop_event>) {
          return dispatch_drop(value);
        } else if constexpr (std::is_same_v<type, window_event>) {
          return dispatch_window(value);
        } else {
          return dispatch_focused(e);
        }
      },
      e);
}

auto event_router::hit_area(widget const &w) const -> rect {
  basalt::size const extent = w.bounds().extent;
  if (!w.has_minimum_target()) {
    return {{}, extent};
  }
  float const target = w.resolve(minimum_target_);
  float const width = std::max(extent.width, target);
  float const height = std::max(extent.height, target);
  return {{(extent.width - width) / 2, (extent.height - height) / 2}, {width, height}};
}

// `local` is inside `w`'s hit area. Children are tested front to back; the
// first whose bounds hold the point and that yields a hit wins, otherwise the
// nearest child whose grown area holds it.
auto event_router::pick(widget &w, point local) const -> widget * {
  bool const inside = rect{{}, w.bounds().extent}.contains(local) && w.contains_point(local);
  if (!inside) {
    return w.hit_testable() ? &w : nullptr;
  }
  widget *grown = nullptr;
  float nearest = std::numeric_limits<float>::infinity();
  for (std::size_t i = w.child_count(); i-- > 0;) {
    widget &c = w.child(i);
    if (!c.visible()) {
      continue;
    }
    point const q = local - c.bounds().origin;
    rect const own{{}, c.bounds().extent};
    if (own.contains(q) && c.contains_point(q)) {
      if (widget *const hit = pick(c, q)) {
        return hit;
      }
      continue;
    }
    if (c.has_minimum_target() && c.hit_testable() && hit_area(c).contains(q)) {
      float const d = distance_squared(q, own);
      if (d < nearest) {
        nearest = d;
        grown = &c;
      }
    }
  }
  if (grown != nullptr) {
    return grown;
  }
  return w.hit_testable() ? &w : nullptr;
}

auto event_router::hit_test(point root_position) const -> widget * {
  widget *const root = tree_->root();
  if (root == nullptr || !root->visible()) {
    return nullptr;
  }
  point const local = root_position - root->bounds().origin;
  if (!rect{{}, root->bounds().extent}.contains(local)) {
    return nullptr;
  }
  widget *hit = pick(*root, local);
  // Disabled widgets pass the pointer to their nearest enabled ancestor.
  for (widget *w = hit; w != nullptr; w = w->parent()) {
    if (!w->enabled()) {
      hit = w->parent();
    }
  }
  while (hit != nullptr && !hit->hit_testable()) {
    hit = hit->parent();
  }
  if (hit != nullptr && tree_->focus().is_blocked(*hit)) {
    return nullptr;
  }
  return hit;
}

auto event_router::hovered() const -> widget * { return tree_->find(hover_target_); }

auto event_router::pressed() const -> widget * {
  return pressed_chain_.empty() ? nullptr : tree_->find(press_target_);
}

auto event_router::pointer_capture() const -> widget * {
  widget *const w = tree_->find(capture_);
  if (w == nullptr || w->state() != widget_state::attached || !w->visible_in_tree() ||
      !w->enabled_in_tree() || tree_->focus().is_blocked(*w)) {
    return nullptr;
  }
  return w;
}

auto event_router::capture_pointer(widget &w) -> bool {
  if (w.tree() != tree_ || w.state() != widget_state::attached || !w.enabled_in_tree() ||
      tree_->focus().is_blocked(w)) {
    return false;
  }
  capture_ = w.id();
  capture_declined_ = false;
  return true;
}

auto event_router::release_pointer() -> void {
  capture_ = {};
  capture_declined_ = true;
}

auto event_router::deliver(widget &target, event const &e, widget const *stop) -> widget_id {
  std::vector<widget_id> chain;
  for (widget *w = &target; w != nullptr; w = w->parent()) {
    chain.push_back(w->id());
    if (w == stop) {
      break;
    }
  }
  std::optional<point> const root_position = event_position(e);
  event_context context{*this, target.id(), root_position};
  // Handlers may change the tree, so each receiver is resolved by id.
  for (auto const id : chain) {
    widget *const w = tree_->find(id);
    if (w == nullptr || w->state() != widget_state::attached || !w->enabled_in_tree()) {
      continue;
    }
    context.current_ = w;
    bool const handled = root_position
                             ? w->on_event(context, with_position(e, w->from_root(*root_position)))
                             : w->on_event(context, e);
    if (handled) {
      return id;
    }
  }
  return {};
}

auto event_router::dispatch_pointer(pointer_event const &e) -> bool {
  switch (e.action) {
  case pointer_action::leave:
    pointer_position_.reset();
    update_hover();
    return true;
  case pointer_action::enter:
    pointer_position_ = e.position;
    update_hover();
    return true;
  case pointer_action::cancel:
    cancel_interaction(e.time);
    return true;
  case pointer_action::move:
  case pointer_action::down:
  case pointer_action::up:
    break;
  }
  pointer_position_ = e.position;
  if (capture_.valid() && pointer_capture() == nullptr) {
    cancel_interaction(e.time);
  }

  widget *const captured = pointer_capture();
  widget *target = captured != nullptr ? captured : hit_test(e.position);
  if (e.action == pointer_action::down) {
    if (captured == nullptr && target != nullptr) {
      widget_id const target_id = target->id();
      tree_->focus().focus_from_pointer(*target);
      // Focus hooks may have changed the tree.
      target = tree_->find(target_id);
    } else {
      tree_->focus().set_keyboard_modality(false);
    }
    if (e.button == pointer_button::primary) {
      press_target_ = target != nullptr ? target->id() : widget_id{};
    }
  }
  update_hover();

  widget_id handler;
  if (target != nullptr) {
    capture_declined_ = false;
    handler = deliver(*target, e);
  }
  if (e.action == pointer_action::down && e.button == pointer_button::primary && handler.valid() &&
      !capture_.valid() && !capture_declined_) {
    if (widget *const w = tree_->find(handler)) {
      capture_pointer(*w);
    }
  }
  if (e.action == pointer_action::up) {
    if (e.button == pointer_button::primary) {
      press_target_ = {};
    }
    if (e.buttons == pointer_button::none) {
      capture_ = {};
      press_target_ = {};
    }
    update_hover();
  }
  return handler.valid();
}

auto event_router::dispatch_wheel(wheel_event const &e) -> bool {
  pointer_position_ = e.position;
  widget *const target = hit_test(e.position);
  return target != nullptr && deliver(*target, e).valid();
}

auto event_router::dispatch_drop(file_drop_event const &e) -> bool {
  if (e.action == file_drop_action::leave) {
    end_drop(e.time);
    return true;
  }
  drop_position_ = e.position;
  widget *const target = hit_test(e.position);
  widget_id const handler = target != nullptr ? deliver(*target, e) : widget_id{};
  widget_id const previous = drop_target_;
  drop_target_ = e.action == file_drop_action::drop ? widget_id{} : handler;
  if (previous.valid() && previous != handler) {
    if (widget *const left = tree_->find(previous);
        left != nullptr && left->state() == widget_state::attached) {
      (void)deliver(*left, file_drop_event{.action = file_drop_action::leave,
                                           .position = e.position,
                                           .time = e.time});
    }
  }
  return handler.valid();
}

auto event_router::end_drop(event_time time) -> void {
  widget_id const previous = std::exchange(drop_target_, {});
  if (widget *const left = tree_->find(previous);
      left != nullptr && left->state() == widget_state::attached) {
    (void)deliver(*left, file_drop_event{.action = file_drop_action::leave,
                                         .position = drop_position_,
                                         .time = time});
  }
}

auto event_router::dispatch_focused(event const &e) -> bool {
  focus_manager &focus = tree_->focus();
  auto const *const key = std::get_if<key_event>(&e);
  if (key != nullptr && key->pressed && !is_modifier_key(key->code)) {
    focus.set_keyboard_modality(true);
  }
  widget *const stop = focus.modal_root();
  widget *target = focus.focused();
  if (target == nullptr) {
    target = stop != nullptr ? stop : tree_->root();
  }
  if (target != nullptr && deliver(*target, e, stop).valid()) {
    return true;
  }
  if (key != nullptr && key->pressed && key->code == key::tab &&
      (key->modifiers == modifier::none || key->modifiers == modifier::shift)) {
    return focus.move_focus(key->modifiers == modifier::shift ? focus_move::previous
                                                              : focus_move::next);
  }
  return false;
}

auto event_router::dispatch_window(window_event const &e) -> bool {
  switch (e.action) {
  case window_action::resized:
    tree_->set_viewport(e.viewport);
    return true;
  case window_action::scale_changed: {
    scale_factors scale = tree_->scale();
    scale.display = e.display_scale;
    return tree_->set_scale(scale);
  }
  case window_action::focus_gained:
    tree_->focus().set_window_active(true);
    return true;
  case window_action::focus_lost:
    tree_->focus().set_window_active(false);
    cancel_interaction(e.time);
    return true;
  case window_action::hidden:
    cancel_interaction(e.time);
    pointer_position_.reset();
    update_hover();
    return false;
  case window_action::close_requested:
  case window_action::shown:
    break;
  }
  return false;
}

auto event_router::cancel_interaction(event_time time) -> void {
  widget *receiver = tree_->find(capture_);
  if (receiver == nullptr) {
    receiver = tree_->find(press_target_);
  }
  capture_ = {};
  press_target_ = {};
  end_drop(time);
  if (receiver != nullptr && receiver->state() == widget_state::attached) {
    (void)deliver(*receiver, pointer_event{.action = pointer_action::cancel,
                                           .position = pointer_position_.value_or(point{}),
                                           .time = time});
  }
  update_hover();
}

auto event_router::update_hover() -> void {
  widget *leaf = pointer_position_ ? hit_test(*pointer_position_) : nullptr;
  // While captured, only the capturing widget's subtree can be hovered.
  if (widget *const captured = pointer_capture(); captured != nullptr && leaf != nullptr &&
                                                  leaf != captured &&
                                                  !captured->is_ancestor_of(*leaf)) {
    leaf = nullptr;
  }
  set_hover(leaf);
  refresh_pressed();
}

auto event_router::set_hover(widget *leaf) -> void {
  hover_target_ = leaf != nullptr ? leaf->id() : widget_id{};
  update_chain(hover_chain_, chain_of(leaf), &interaction_state::hovered);
}

// The press shows only while the pointer is over the widget it started on.
auto event_router::refresh_pressed() -> void {
  widget *const target = tree_->find(press_target_);
  bool const active = target != nullptr && std::ranges::contains(hover_chain_, target->id());
  update_chain(pressed_chain_, active ? chain_of(target) : std::vector<widget_id>{},
               &interaction_state::pressed);
}

auto event_router::update_chain(std::vector<widget_id> &chain, std::vector<widget_id> next,
                                bool interaction_state::*flag) -> void {
  for (auto const id : chain) {
    if (!std::ranges::contains(next, id)) {
      if (widget *const w = tree_->find(id)) {
        w->set_interaction_flag(flag, false);
      }
    }
  }
  for (auto const id : next) {
    if (widget *const w = tree_->find(id)) {
      w->set_interaction_flag(flag, true);
    }
  }
  chain = std::move(next);
}

} // namespace basalt
