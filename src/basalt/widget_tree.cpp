#include "basalt/widget_tree.h"

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

namespace basalt {

namespace {

auto encloses(rect outer, rect inner) -> bool {
  return inner.left() >= outer.left() && inner.top() >= outer.top() &&
         inner.right() <= outer.right() && inner.bottom() <= outer.bottom();
}

} // namespace

widget_tree::widget_tree(basalt::theme theme, scale_factors scale)
    : theme_(std::move(theme)), scale_(is_valid(scale) ? scale : scale_factors{}) {}

widget_tree::~widget_tree() {
  frame_request_handler_ = nullptr;
  focus_.reset();
  if (root_) {
    root_->detach_subtree();
  }
  root_.reset();
  collect_garbage();
}

auto widget_tree::replace_root(std::unique_ptr<widget> root) -> void {
  if (!root) {
    throw std::invalid_argument("basalt::widget_tree: cannot set a null root");
  }
  // Destroyed on return, after the new root is attached.
  auto const old = take_root();
  root_ = std::move(root);
  root_->bounds_ = {{}, viewport_};
  root_->mark_needs_layout();
  root_->attach_subtree(*this);
  add_damage({{}, viewport_});
  request_frame();
}

auto widget_tree::take_root() -> std::unique_ptr<widget> {
  if (!root_ || root_->state_ == widget_state::detaching) {
    return nullptr;
  }
  add_damage({{}, viewport_});
  root_->detach_subtree();
  return std::exchange(root_, nullptr);
}

auto widget_tree::find(widget_id id) const -> widget * {
  auto const it = registry_.find(id);
  return it == registry_.end() ? nullptr : it->second;
}

auto widget_tree::set_viewport(basalt::size viewport) -> void {
  if (viewport == viewport_) {
    return;
  }
  viewport_ = viewport;
  if (root_) {
    root_->set_bounds({{}, viewport_});
  }
}

auto widget_tree::set_theme(basalt::theme theme) -> void {
  if (theme == theme_) {
    return;
  }
  theme_ = std::move(theme);
  environment_changed();
}

auto widget_tree::set_text_shaper(text::text_shaper const *shaper) -> void {
  if (shaper != shaper_) {
    shaper_ = shaper;
    environment_changed();
  }
}

auto widget_tree::set_scale(scale_factors scale) -> bool {
  if (!is_valid(scale)) {
    return false;
  }
  if (scale != scale_) {
    scale_ = scale;
    environment_changed();
  }
  return true;
}

auto widget_tree::environment_changed() -> void {
  if (!root_) {
    return;
  }
  std::vector<widget_id> ids;
  ids.reserve(registry_.size());
  root_->for_each_in_subtree([&](widget &w) {
    w.needs_layout_ = true;
    w.drop_measurements();
    w.descendant_needs_layout_ = !w.children_.empty();
    ids.push_back(w.id_);
  });
  add_damage({{}, viewport_});
  request_frame();
  for (auto const id : ids) {
    if (widget *const w = find(id)) {
      w->on_environment_changed();
    }
  }
}

auto widget_tree::needs_layout() const -> bool { return root_ && root_->subtree_needs_layout(); }

auto widget_tree::update_layout() -> bool {
  for (std::size_t pass = 0; pass < max_layout_passes && needs_layout(); ++pass) {
    layout_subtree(*root_);
  }
  return !needs_layout();
}

// Flags are cleared before the hook runs so that `on_layout` can invalidate
// again; anything it marks above the current widget is left for the next pass.
auto widget_tree::layout_subtree(widget &w) -> void {
  if (w.needs_layout_) {
    w.needs_layout_ = false;
    w.on_layout();
  }
  if (!w.descendant_needs_layout_) {
    return;
  }
  w.descendant_needs_layout_ = false;
  for (std::size_t i = 0; i < w.children_.size(); ++i) {
    widget &child = *w.children_[i];
    if (child.subtree_needs_layout()) {
      layout_subtree(child);
    }
  }
}

auto widget_tree::take_damage() -> std::vector<rect> {
  std::vector<rect> taken;
  taken.swap(damage_);
  frame_requested_ = false;
  if (needs_frame()) {
    request_frame();
  }
  return taken;
}

auto widget_tree::set_frame_request_handler(std::function<void()> handler) -> void {
  frame_request_handler_ = std::move(handler);
  frame_requested_ = false;
  if (needs_frame()) {
    request_frame();
  }
}

auto widget_tree::request_frame() -> void {
  if (frame_requested_) {
    return;
  }
  frame_requested_ = true;
  if (frame_request_handler_) {
    frame_request_handler_();
  }
}

auto widget_tree::add_damage(rect area) -> void {
  area = intersection(area, {{}, viewport_});
  if (area.empty()) {
    return;
  }
  request_frame();
  if (std::ranges::any_of(damage_, [&](rect r) { return encloses(r, area); })) {
    return;
  }
  std::erase_if(damage_, [&](rect r) { return encloses(area, r); });
  damage_.push_back(area);
  if (damage_.size() > max_damage_rects) {
    rect merged;
    for (auto const r : damage_) {
      merged = bounding_union(merged, r);
    }
    damage_.assign(1, merged);
  }
}

auto widget_tree::destroy_later(widget &w) -> bool {
  if (w.tree_ != this) {
    return false;
  }
  auto owned = &w == root_.get() ? take_root() : w.parent_->remove_child(w);
  if (!owned) {
    return false;
  }
  graveyard_.push_back(std::move(owned));
  return true;
}

auto widget_tree::collect_garbage() -> void {
  std::vector<std::unique_ptr<widget>> dead;
  dead.swap(graveyard_);
}

auto widget_tree::register_widget(widget &w) -> void { registry_.emplace(w.id_, &w); }

auto widget_tree::unregister_widget(widget &w) -> void {
  registry_.erase(w.id_);
  std::erase(animating_, w.id_);
}

auto widget_tree::add_animation(widget &w) -> void {
  if (std::ranges::find(animating_, w.id_) == animating_.end()) {
    animating_.push_back(w.id_);
  }
  request_frame();
}

auto widget_tree::advance_animations(std::chrono::nanoseconds now) -> void {
  using namespace std::chrono_literals;
  if (animating_.empty()) {
    last_animation_time_.reset();
    return;
  }
  // The first frame of a run has no earlier frame to measure from. A long gap,
  // such as a stalled process, counts as a few frames so that motion does not jump.
  constexpr std::chrono::nanoseconds nominal = 16'667us;
  constexpr std::chrono::nanoseconds longest = 100ms;
  auto const elapsed =
      last_animation_time_ ? std::clamp(now - *last_animation_time_, 0ns, longest) : nominal;
  last_animation_time_ = now;

  std::vector<widget_id> running;
  running.swap(animating_);
  for (auto const id : running) {
    if (widget *const w = find(id); w != nullptr && w->on_animation_frame(elapsed)) {
      add_animation(*w);
    }
  }
}

} // namespace basalt
