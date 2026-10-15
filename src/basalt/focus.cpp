#include "basalt/focus.h"

#include "basalt/widget_tree.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <ranges>
#include <utility>
#include <vector>

namespace basalt {

namespace {

// One Tab stop: `owner` is the widget itself, or the group scope it stands in
// for.
struct stop {
  widget *owner;
  widget *target;
  std::size_t order;
};

// The stops under one traversal root, plus where the focused widget sits
// among them.
struct stop_list {
  std::vector<stop> stops;
  // The stop holding focus.
  std::optional<std::size_t> current;
  // Tree order of the focused widget (or of the group that holds it), for
  // focus that is not on a stop.
  std::optional<std::size_t> focused_order;
};

auto is_tab_stop(widget const &w) -> bool {
  return focus_by_tab(w.focus_policy()) && w.state() == widget_state::attached;
}

template <class EntryOf>
auto collect_stops(widget &root, widget const *focused, EntryOf const &entry_of) -> stop_list {
  stop_list result;
  if (!root.visible_in_tree()) {
    return result;
  }
  std::size_t counter = 0;
  auto visit = [&](this auto const &self, widget &w, bool is_root) -> void {
    std::size_t const order = counter++;
    if (!w.visible()) {
      return;
    }
    if (&w == focused) {
      result.focused_order = order;
    }
    if (!is_root && w.focus_scope() == focus_scope::group) {
      bool const holds_focus = focused != nullptr && w.is_ancestor_of(*focused);
      if (holds_focus) {
        result.focused_order = order;
      }
      if (widget *const target = entry_of(w)) {
        if (holds_focus) {
          result.current = result.stops.size();
        }
        result.stops.push_back({&w, target, order});
      }
      return;
    }
    if (is_tab_stop(w)) {
      if (&w == focused) {
        result.current = result.stops.size();
      }
      result.stops.push_back({&w, &w, order});
    }
    for (auto &c : w.children()) {
      self(c, false);
    }
  };
  visit(root, true);
  return result;
}

// The index `move` selects, or none when it runs off an end without `wrap`.
auto pick(stop_list const &list, focus_move move, bool wrap) -> std::optional<std::size_t> {
  std::size_t const n = list.stops.size();
  switch (move) {
  case focus_move::first:
    return 0;
  case focus_move::last:
    return n - 1;
  case focus_move::next:
    if (list.current) {
      if (*list.current + 1 < n) {
        return *list.current + 1;
      }
    } else if (list.focused_order) {
      for (std::size_t i = 0; i < n; ++i) {
        if (list.stops[i].order > *list.focused_order) {
          return i;
        }
      }
    } else {
      return 0;
    }
    return wrap ? std::optional<std::size_t>{0} : std::nullopt;
  case focus_move::previous:
    if (list.current) {
      if (*list.current > 0) {
        return *list.current - 1;
      }
    } else if (list.focused_order) {
      for (std::size_t i = n; i-- > 0;) {
        if (list.stops[i].order < *list.focused_order) {
          return i;
        }
      }
    } else {
      return n - 1;
    }
    return wrap ? std::optional<std::size_t>{n - 1} : std::nullopt;
  }
  return std::nullopt;
}

} // namespace

auto focus_manager::focused() const -> widget * {
  return focused_.valid() ? tree_->find(focused_) : nullptr;
}

auto focus_manager::set_focus(widget &w, focus_reason reason) -> bool {
  if (w.tree() != tree_ || !w.accepts_focus() || is_blocked(w)) {
    return false;
  }
  change_focus(&w, reason);
  return true;
}

auto focus_manager::clear_focus() -> void { change_focus(nullptr, focus_reason::programmatic); }

auto focus_manager::focus_from_pointer(widget &target) -> bool {
  keyboard_modality_ = false;
  for (widget *w = &target; w != nullptr; w = w->parent()) {
    if (w->focus_policy() == focus_policy::none) {
      continue;
    }
    // The innermost focusable widget decides, so pressing a Tab-only control
    // inside an editor does not move focus to the editor.
    if (focus_by_click(w->focus_policy()) && w->accepts_focus() && !is_blocked(*w)) {
      change_focus(w, focus_reason::pointer);
      return true;
    }
    break;
  }
  refresh_flags();
  return false;
}

auto focus_manager::change_focus(widget *next, focus_reason reason) -> void {
  if (reason == focus_reason::keyboard) {
    keyboard_modality_ = true;
  } else if (reason == focus_reason::pointer) {
    keyboard_modality_ = false;
  }
  widget_id const next_id = next != nullptr ? next->id() : widget_id{};
  bool const changed = next_id != focused_;
  focused_ = next_id;
  if (next != nullptr) {
    remember_in_groups(*next);
  }
  refresh_flags();
  if (changed && changed_handler_) {
    changed_handler_(focused());
  }
}

auto focus_manager::refresh_flags() -> void {
  widget *const current = focused();
  std::vector<widget_id> next_chain;
  for (widget *w = current; w != nullptr; w = w->parent()) {
    next_chain.push_back(w->id());
  }
  bool const visible = focus_visible();
  // Hooks run from here may change the tree, so widgets are resolved by id.
  for (auto const id : chain_) {
    if (std::ranges::contains(next_chain, id)) {
      continue;
    }
    if (widget *const w = tree_->find(id)) {
      if (w->interaction_.focus_visible) {
        damage_ring(*w);
      }
      w->set_interaction_flag(&interaction_state::focus_visible, false);
      w->set_interaction_flag(&interaction_state::focused, false);
      w->set_interaction_flag(&interaction_state::focus_within, false);
    }
  }
  for (auto const id : next_chain) {
    widget *const w = tree_->find(id);
    if (w == nullptr) {
      continue;
    }
    bool const is_focused = id == focused_;
    bool const shows_ring = is_focused && visible;
    w->set_interaction_flag(&interaction_state::focus_within, true);
    w->set_interaction_flag(&interaction_state::focused, is_focused);
    if (w->interaction_.focus_visible != shows_ring) {
      damage_ring(*w);
      w->set_interaction_flag(&interaction_state::focus_visible, shows_ring);
    }
  }
  chain_ = std::move(next_chain);
}

auto focus_manager::damage_ring(widget const &w) -> void {
  if (w.tree() != tree_ || !w.visible_in_tree()) {
    return;
  }
  if (auto const shape = w.focus_ring()) {
    focus_ring const ring = make_focus_ring(w, *shape);
    tree_->add_damage(intersection(ring.outer, ring.clip));
  }
}

auto focus_manager::remember_in_groups(widget &w) -> void {
  for (widget *p = w.parent(); p != nullptr; p = p->parent()) {
    if (p->focus_scope() == focus_scope::group) {
      group_entries_[p->id()] = w.id();
    }
  }
}

auto focus_manager::set_group_entry(widget &group, widget &entry) -> bool {
  if (group.focus_scope() != focus_scope::group || !group.is_ancestor_of(entry)) {
    return false;
  }
  group_entries_[group.id()] = entry.id();
  return true;
}

auto focus_manager::group_entry(widget &group) const -> widget * {
  if (auto const it = group_entries_.find(group.id()); it != group_entries_.end()) {
    widget *const remembered = tree_->find(it->second);
    if (remembered != nullptr && group.is_ancestor_of(*remembered) && is_tab_stop(*remembered) &&
        remembered->visible_in_tree()) {
      return remembered;
    }
  }
  auto const inner = collect_stops(group, nullptr, [this](widget &g) { return group_entry(g); });
  return inner.stops.empty() ? nullptr : inner.stops.front().target;
}

auto focus_manager::traversal_root() const -> widget * {
  widget *const modal = modal_root();
  return modal != nullptr ? modal : tree_->root();
}

auto focus_manager::move_among(widget &root, focus_move move, bool wrap, focus_reason reason)
    -> bool {
  auto const list = collect_stops(root, focused(), [this](widget &g) { return group_entry(g); });
  if (list.stops.empty()) {
    return false;
  }
  auto const index = pick(list, move, wrap);
  return index && set_focus(*list.stops[*index].target, reason);
}

auto focus_manager::move_focus(focus_move move, focus_reason reason) -> bool {
  widget *const root = traversal_root();
  return root != nullptr && move_among(*root, move, true, reason);
}

auto focus_manager::move_focus_in_group(focus_move move, bool wrap, focus_reason reason) -> bool {
  widget *const current = focused();
  if (current == nullptr) {
    return false;
  }
  for (widget *g = current->parent(); g != nullptr; g = g->parent()) {
    if (g->focus_scope() == focus_scope::group) {
      return move_among(*g, move, wrap, reason);
    }
  }
  return false;
}

auto focus_manager::tab_order(widget const *within) const -> std::vector<widget *> {
  widget *const root = within != nullptr ? tree_->find(within->id()) : traversal_root();
  if (root == nullptr) {
    return {};
  }
  auto const list = collect_stops(*root, nullptr, [this](widget &g) { return group_entry(g); });
  std::vector<widget *> order;
  order.reserve(list.stops.size());
  for (auto const &s : list.stops) {
    order.push_back(s.target);
  }
  return order;
}

auto focus_manager::push_modal(widget &modal) -> bool {
  if (modal.tree() != tree_ || modal.state() != widget_state::attached) {
    return false;
  }
  if (modal_root() == &modal) {
    return true;
  }
  std::erase_if(modals_, [&](modal_entry const &e) { return e.modal == modal.id(); });
  modals_.push_back({modal.id(), focused_});
  widget *const current = focused();
  if (current != nullptr && (current == &modal || modal.is_ancestor_of(*current))) {
    return true;
  }
  if (!move_focus(focus_move::first, focus_reason::programmatic)) {
    change_focus(modal.accepts_focus() ? &modal : nullptr, focus_reason::programmatic);
  }
  return true;
}

auto focus_manager::pop_modal(widget &modal) -> bool {
  auto const it =
      std::ranges::find_if(modals_, [&](modal_entry const &e) { return e.modal == modal.id(); });
  if (it == modals_.end()) {
    return false;
  }
  bool const was_active = it + 1 == modals_.end();
  widget_id const restore = it->restore;
  modals_.erase(it);
  if (was_active) {
    widget *const target = tree_->find(restore);
    if (target != nullptr && target->accepts_focus() && !is_blocked(*target)) {
      change_focus(target, focus_reason::programmatic);
    }
  }
  return true;
}

auto focus_manager::modal_root() const -> widget * {
  return modals_.empty() ? nullptr : tree_->find(modals_.back().modal);
}

auto focus_manager::is_blocked(widget const &w) const -> bool {
  widget const *const modal = modal_root();
  return modal != nullptr && &w != modal && !modal->is_ancestor_of(w);
}

auto focus_manager::set_keyboard_modality(bool keyboard) -> void {
  if (keyboard != keyboard_modality_) {
    keyboard_modality_ = keyboard;
    refresh_flags();
  }
}

auto focus_manager::set_window_active(bool active) -> void {
  if (active != window_active_) {
    window_active_ = active;
    refresh_flags();
  }
}

auto focus_manager::focus_visible() const -> bool {
  return keyboard_modality_ && window_active_ && focused() != nullptr;
}

auto focus_manager::set_focus_changed_handler(std::function<void(widget *)> handler) -> void {
  changed_handler_ = std::move(handler);
}

auto focus_manager::fall_back(widget *from) -> void {
  for (widget *w = from; w != nullptr; w = w->parent()) {
    if (w->accepts_focus() && !is_blocked(*w)) {
      change_focus(w, focus_reason::programmatic);
      return;
    }
  }
  if (modal_root() != nullptr && move_focus(focus_move::first, focus_reason::programmatic)) {
    return;
  }
  change_focus(nullptr, focus_reason::programmatic);
}

auto focus_manager::subtree_detached(widget *former_parent) -> void {
  std::erase_if(group_entries_, [this](auto const &entry) {
    return !tree_->contains(entry.first) || !tree_->contains(entry.second);
  });
  auto const gone = [this](modal_entry const &e) { return !tree_->contains(e.modal); };
  std::optional<widget_id> restore;
  while (!modals_.empty() && gone(modals_.back())) {
    restore = modals_.back().restore;
    modals_.pop_back();
  }
  std::erase_if(modals_, gone);
  if (restore) {
    widget *const target = tree_->find(*restore);
    if (target != nullptr && target->accepts_focus() && !is_blocked(*target)) {
      change_focus(target, focus_reason::programmatic);
    }
  }
  if (focused_.valid() && !tree_->contains(focused_)) {
    fall_back(former_parent);
  }
}

auto focus_manager::subtree_hidden(widget &top) -> void {
  widget *const current = focused();
  if (current != nullptr && (current == &top || top.is_ancestor_of(*current))) {
    fall_back(top.parent());
  }
}

auto focus_manager::reset() -> void {
  focused_ = {};
  chain_.clear();
  modals_.clear();
  group_entries_.clear();
  changed_handler_ = nullptr;
}

} // namespace basalt
