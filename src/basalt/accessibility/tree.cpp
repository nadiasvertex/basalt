#include "basalt/accessibility/tree.h"

#include <algorithm>
#include <ranges>
#include <utility>

namespace basalt::accessibility {

auto tree::root() const -> node const * { return nodes_.empty() ? nullptr : &nodes_.front(); }

auto tree::find(node_id id) const -> node const * {
  auto const it = index_.find(id);
  return it == index_.end() ? nullptr : &nodes_[it->second];
}

auto tree::parent_of(node_id id) const -> node const * {
  node const *const n = find(id);
  return n == nullptr ? nullptr : find(n->parent);
}

namespace {

// Collects nodes into a scratch table, remembers each one's children in
// reading order, then flattens the table into reading order.
struct builder {
  widget_tree const &source;
  std::vector<node> table;
  std::unordered_map<std::size_t, std::vector<std::size_t>> children_of;

  // Returns the ids of the nodes `w` contributes to its parent: its own, or
  // its children's when it has no role.
  auto visit(widget const &w, bool is_root) -> std::vector<std::size_t> {
    if (!w.visible()) {
      return {};
    }
    auto semantics = w.accessibility_info();
    if (semantics.kind == role::none && is_root) {
      semantics.kind = role::window;
    }
    if (semantics.kind == role::none) {
      std::vector<std::size_t> hoisted;
      for (auto const &child : w.children()) {
        auto below = visit(child, false);
        hoisted.insert(hoisted.end(), below.begin(), below.end());
      }
      return hoisted;
    }

    node n;
    n.id = w.id();
    n.bounds = w.bounds_in_root();
    n.focusable = w.accepts_focus();
    n.focused = w.has_focus();
    n.disabled = !w.enabled_in_tree();
    n.blocked = source.focus().is_blocked(w);
    if (n.focusable) {
      semantics.actions.insert(action::focus);
    }
    n.semantics = std::move(semantics);
    std::size_t const slot = table.size();
    table.push_back(std::move(n));

    std::vector<std::size_t> kids;
    for (auto const &child : w.children()) {
      auto below = visit(child, false);
      kids.insert(kids.end(), below.begin(), below.end());
    }
    std::ranges::stable_sort(kids, {},
                             [&](std::size_t k) { return table[k].semantics.reading_order; });
    for (std::size_t const k : kids) {
      table[k].parent = table[slot].id;
      table[slot].children.push_back(table[k].id);
    }
    children_of[slot] = std::move(kids);
    return {slot};
  }

  auto flatten(std::size_t slot, std::vector<node> &out) -> void {
    out.push_back(std::move(table[slot]));
    for (std::size_t const k : children_of[slot]) {
      flatten(k, out);
    }
  }
};

} // namespace

auto build(widget_tree const &source) -> tree {
  tree result;
  widget const *const root = source.root();
  if (root == nullptr) {
    return result;
  }
  builder b{source, {}, {}};
  auto const tops = b.visit(*root, true);
  if (tops.empty()) {
    return result;
  }
  // The root widget always yields exactly one node unless it is hidden.
  b.flatten(tops.front(), result.nodes_);
  for (std::size_t i = 0; i < result.nodes_.size(); ++i) {
    result.index_.emplace(result.nodes_[i].id, i);
  }

  // Resolve the focused and modal widgets to the nearest node.
  auto const nearest = [&](widget const *w) -> node_id {
    for (; w != nullptr; w = w->parent()) {
      if (result.index_.contains(w->id())) {
        return w->id();
      }
    }
    return {};
  };
  result.focused_ = nearest(source.focus().focused());
  result.modal_ = nearest(source.focus().modal_root());
  return result;
}

auto diff(tree const &before, tree const &after) -> tree_diff {
  tree_diff d;
  for (node const &n : after.nodes()) {
    node const *const old = before.find(n.id);
    if (old == nullptr) {
      d.added.push_back(n.id);
    } else if (!(*old == n)) {
      d.changed.push_back(n.id);
    }
  }
  for (node const &n : before.nodes()) {
    if (after.find(n.id) == nullptr) {
      d.removed.push_back(n.id);
    }
  }
  d.focus_changed = before.focused() != after.focused();
  return d;
}

auto perform_action(widget_tree &target, node_id id, action_request const &request) -> bool {
  widget *const w = target.find(id);
  return w != nullptr && w->perform_accessibility_action(request);
}

auto audit(tree const &snapshot) -> std::vector<issue> {
  std::vector<issue> issues;
  for (node const &n : snapshot.nodes()) {
    info const &s = n.semantics;
    bool const interactive = is_interactive(s.kind);
    if (requires_name(s.kind) && s.name.empty() && s.labelled_by.empty()) {
      issues.push_back({n.id, issue_kind::missing_name});
    }
    if (interactive && !n.focusable) {
      issues.push_back({n.id, issue_kind::interactive_not_focusable});
    }
    // Focus alone does not make a control operable.
    auto operations = s.actions;
    operations.erase(action::focus);
    if (interactive && operations.empty()) {
      issues.push_back({n.id, issue_kind::missing_actions});
    }
    if (interactive && n.bounds.empty()) {
      issues.push_back({n.id, issue_kind::empty_bounds});
    }
    for (auto const *refs : {&s.labelled_by, &s.described_by}) {
      if (std::ranges::any_of(*refs, [&](node_id r) { return snapshot.find(r) == nullptr; })) {
        issues.push_back({n.id, issue_kind::dangling_reference});
      }
    }
  }
  return issues;
}

} // namespace basalt::accessibility
