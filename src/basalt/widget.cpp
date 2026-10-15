#include "basalt/widget.h"

#include "basalt/render_context.h"
#include "basalt/widget_tree.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace basalt {

namespace {

auto next_widget_id() -> widget_id {
  static std::atomic<std::uint64_t> last{0};
  return {last.fetch_add(1, std::memory_order_relaxed) + 1};
}

} // namespace

widget::widget() : id_(next_widget_id()) {}

widget::~widget() = default;

auto widget::is_ancestor_of(widget const &other) const -> bool {
  for (widget const *w = other.parent_; w != nullptr; w = w->parent_) {
    if (w == this) {
      return true;
    }
  }
  return false;
}

auto widget::index_of(widget const &child) const -> std::optional<std::size_t> {
  auto const it = std::ranges::find_if(children_, [&](auto const &c) { return c.get() == &child; });
  if (it == children_.end()) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(it - children_.begin());
}

auto widget::insert_owned(std::size_t index, std::unique_ptr<widget> child) -> void {
  if (!child) {
    throw std::invalid_argument("basalt::widget: cannot add a null child");
  }
  widget &added = *child;
  index = std::min(index, children_.size());
  children_.insert(children_.begin() + static_cast<std::ptrdiff_t>(index), std::move(child));
  added.parent_ = this;
  added.mark_needs_layout();
  invalidate_layout();
  if (tree_ != nullptr && (state_ == widget_state::attached || state_ == widget_state::attaching)) {
    added.attach_subtree(*tree_);
    added.invalidate_paint();
  }
}

auto widget::clear_children() -> void {
  std::vector<widget *> doomed;
  doomed.reserve(children_.size());
  for (auto const &child : children_) {
    doomed.push_back(child.get());
  }
  for (widget *child : doomed) {
    if (tree_ != nullptr) {
      (void)tree_->destroy_later(*child);
    } else {
      (void)remove_child(*child);
    }
  }
}

auto widget::remove_child(widget &child) -> std::unique_ptr<widget> {
  if (child.parent_ != this || child.state_ == widget_state::detaching) {
    return nullptr;
  }
  child.invalidate_paint();
  if (child.tree_ != nullptr) {
    child.detach_subtree();
  }
  // Lifecycle hooks may have reordered the children.
  auto const index = index_of(child);
  if (!index) {
    return nullptr;
  }
  auto const it = children_.begin() + static_cast<std::ptrdiff_t>(*index);
  auto owned = std::move(*it);
  children_.erase(it);
  owned->parent_ = nullptr;
  invalidate_layout();
  on_child_removed(*owned);
  return owned;
}

auto widget::remove_from_parent() -> std::unique_ptr<widget> {
  if (parent_ == nullptr) {
    return nullptr;
  }
  return parent_->remove_child(*this);
}

auto widget::move_child(widget &child, std::size_t index) -> bool {
  auto const from = index_of(child);
  if (!from) {
    return false;
  }
  index = std::min(index, children_.size() - 1);
  if (index == *from) {
    return true;
  }
  auto owned = std::move(children_[*from]);
  children_.erase(children_.begin() + static_cast<std::ptrdiff_t>(*from));
  children_.insert(children_.begin() + static_cast<std::ptrdiff_t>(index), std::move(owned));
  invalidate_layout();
  child.invalidate_paint();
  return true;
}

auto widget::set_bounds(rect bounds) -> void {
  if (bounds == bounds_) {
    return;
  }
  bool const resized = bounds.extent != bounds_.extent;
  invalidate_paint();
  bounds_ = bounds;
  invalidate_paint();
  if (resized) {
    mark_needs_layout();
  }
}

auto widget::measure(constraints const &limits) const -> basalt::size {
  for (auto const &entry : measured_) {
    if (entry && entry->limits == limits) {
      return entry->result;
    }
  }
  auto const result = limits.constrain(on_measure(limits));
  measured_[measured_next_] = measured_size{limits, result};
  measured_next_ = (measured_next_ + 1) % measured_.size();
  return result;
}

auto widget::drop_measurements() const -> void { measured_.fill(std::nullopt); }

auto widget::scale() const -> scale_factors {
  return tree_ != nullptr ? tree_->scale() : scale_factors{};
}

auto widget::resolve(length l) const -> float { return to_layout(l, scale()); }

auto widget::resolve(length_insets const &edges) const -> insets {
  return to_layout(edges, scale());
}

auto widget::bounds_in_root() const -> rect { return {to_root({}), bounds_.extent}; }

auto widget::to_root(point local) const -> point {
  for (widget const *w = this; w != nullptr; w = w->parent_) {
    local = local + w->bounds_.origin;
  }
  return local;
}

auto widget::from_root(point root) const -> point { return root - to_root({}); }

auto widget::visible_in_tree() const -> bool {
  for (widget const *w = this; w != nullptr; w = w->parent_) {
    if (!w->visible_) {
      return false;
    }
  }
  return true;
}

auto widget::set_visible(bool visible) -> void {
  if (visible == visible_) {
    return;
  }
  if (!visible) {
    invalidate_paint();
  }
  visible_ = visible;
  if (visible) {
    invalidate_paint();
  }
  if (parent_ != nullptr) {
    parent_->invalidate_layout();
  }
  if (!visible && tree_ != nullptr) {
    tree_->focus().subtree_hidden(*this);
  }
}

auto widget::enabled_in_tree() const -> bool {
  for (widget const *w = this; w != nullptr; w = w->parent_) {
    if (!w->enabled_) {
      return false;
    }
  }
  return true;
}

auto widget::set_enabled(bool enabled) -> void {
  if (enabled != enabled_) {
    enabled_ = enabled;
    invalidate_paint();
  }
}

auto widget::contains_point(point local) const -> bool {
  return rect{{}, bounds_.extent}.contains(local);
}

auto widget::accepts_focus() const -> bool {
  return tree_ != nullptr && state_ == widget_state::attached &&
         focus_policy_ != focus_policy::none && visible_in_tree();
}

auto widget::request_focus(focus_reason reason) -> bool {
  return tree_ != nullptr && tree_->focus().set_focus(*this, reason);
}

auto widget::perform_accessibility_action(accessibility::action_request const &request) -> bool {
  if (tree_ == nullptr || !visible_in_tree() || tree_->focus().is_blocked(*this)) {
    return false;
  }
  if (request.kind == accessibility::action::focus) {
    return request_focus(focus_reason::programmatic);
  }
  if (!enabled_in_tree() || !accessibility_info().actions.contains(request.kind)) {
    return false;
  }
  return on_accessibility_action(request);
}

auto widget::paint(render_context &context) const -> void { on_paint(context); }

auto widget::paint_overlay(render_context &context) const -> void { on_paint_overlay(context); }

auto widget::focus_ring() const -> std::optional<focus_ring_shape> {
  basalt::theme const fallback;
  basalt::theme const &theme = tree_ != nullptr ? tree_->theme() : fallback;
  return focus_ring_shape{{{}, bounds_.extent}, resolve(theme.sizes.radius_default)};
}

auto widget::set_interaction_flag(bool interaction_state::*flag, bool value) -> void {
  if (interaction_.*flag == value) {
    return;
  }
  interaction_state const previous = interaction_;
  interaction_.*flag = value;
  on_interaction_changed(previous);
}

auto widget::invalidate_layout() -> void {
  mark_needs_layout();
  for (widget *w = parent_; w != nullptr; w = w->parent_) {
    w->needs_layout_ = true;
    w->drop_measurements();
  }
}

auto widget::request_animation_frame() -> void {
  if (tree_ != nullptr) {
    tree_->add_animation(*this);
  }
}

auto widget::invalidate_paint() -> void {
  invalidate_paint({{}, bounds_.extent});
  // The focus ring is drawn outside the bounds.
  if (interaction_.focus_visible && tree_ != nullptr) {
    tree_->focus().damage_ring(*this);
  }
}

auto widget::invalidate_paint(rect local) -> void {
  if (tree_ == nullptr || !visible_in_tree()) {
    return;
  }
  // Walk up in each parent's coordinates, clipping to every ancestor because
  // children never paint outside them.
  rect area = intersection(local, {{}, bounds_.extent});
  widget const *top = this;
  for (; top->parent_ != nullptr && !area.empty(); top = top->parent_) {
    area = intersection(translated(area, top->bounds_.origin), {{}, top->parent_->bounds_.extent});
  }
  if (!area.empty()) {
    tree_->add_damage(translated(area, top->bounds_.origin));
  }
}

// Keeps the invariant that every ancestor of a widget needing layout has
// `descendant_needs_layout_` set, which lets the walk stop early.
auto widget::mark_needs_layout() -> void {
  needs_layout_ = true;
  drop_measurements();
  for (widget *w = parent_; w != nullptr && !w->descendant_needs_layout_; w = w->parent_) {
    w->descendant_needs_layout_ = true;
  }
  if (tree_ != nullptr) {
    tree_->request_frame();
  }
}

auto widget::for_each_in_subtree(std::function<void(widget &)> const &visit) -> void {
  visit(*this);
  for (std::size_t i = 0; i < children_.size(); ++i) {
    children_[i]->for_each_in_subtree(visit);
  }
}

auto widget::attach_subtree(widget_tree &tree) -> void {
  // Register the whole subtree before running any hook, so hooks can find
  // their descendants. Hooks may remove widgets, so they are resolved by id.
  std::vector<widget_id> order;
  for_each_in_subtree([&](widget &w) {
    w.tree_ = &tree;
    w.state_ = widget_state::attaching;
    // Sizes measured while detached used the default scale.
    w.drop_measurements();
    tree.register_widget(w);
    order.push_back(w.id_);
  });
  for (auto const id : order) {
    widget *const w = tree.find(id);
    if (w != nullptr && w->state_ == widget_state::attaching) {
      w->state_ = widget_state::attached;
      w->on_attached();
    }
  }
}

auto widget::detach_subtree() -> void {
  // Children first. Widgets in the detaching state cannot be removed, so the
  // pointers stay valid while hooks run.
  struct entry {
    widget *w;
    bool notify;
  };
  std::vector<entry> order;
  std::function<void(widget &)> collect = [&](widget &w) {
    for (auto const &c : w.children_) {
      if (c->tree_ != nullptr) {
        collect(*c);
      }
    }
    order.push_back({&w, w.state_ == widget_state::attached});
    w.state_ = widget_state::detaching;
  };
  collect(*this);
  for (auto const &e : order) {
    if (e.notify) {
      e.w->on_detached();
    }
  }
  widget_tree &tree = *tree_;
  for (auto const &e : order) {
    tree.unregister_widget(*e.w);
    e.w->tree_ = nullptr;
    e.w->state_ = widget_state::detached;
    // Hover, press and focus belong to the tree being left.
    e.w->interaction_ = {};
  }
  tree.focus().subtree_detached(parent_);
}

} // namespace basalt
