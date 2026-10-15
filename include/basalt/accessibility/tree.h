#pragma once

#include "basalt/accessibility/node.h"
#include "basalt/widget_tree.h"

#include <cstddef>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace basalt::accessibility {

// An immutable snapshot of the semantic tree of one widget tree.
//
// Hidden widgets and their subtrees are left out. A widget whose role is
// `role::none` is left out too, and its children join its parent. A root
// widget without a role becomes an unnamed `role::window`, so there is
// always a single root. Siblings follow tree order unless `info::reading_order`
// says otherwise, and `nodes()` lists nodes in reading order, parents before
// children.
//
// Snapshots do not refer back to the widgets, so a platform bridge can keep
// one, build the next, and compare them with `diff`.
class tree {
public:
  tree() = default;

  [[nodiscard]] auto empty() const -> bool { return nodes_.empty(); }
  [[nodiscard]] auto size() const -> std::size_t { return nodes_.size(); }
  [[nodiscard]] auto nodes() const -> std::span<node const> { return nodes_; }
  // Null when the tree is empty.
  [[nodiscard]] auto root() const -> node const *;
  // Null when no node has `id`.
  [[nodiscard]] auto find(node_id id) const -> node const *;
  [[nodiscard]] auto parent_of(node_id id) const -> node const *;
  // The node holding keyboard focus: the focused widget's nearest ancestor
  // with a node. Invalid when nothing has focus.
  [[nodiscard]] auto focused() const -> node_id { return focused_; }
  // The active modal's node, or invalid.
  [[nodiscard]] auto modal() const -> node_id { return modal_; }

private:
  friend auto build(widget_tree const &) -> tree;

  std::vector<node> nodes_;
  std::unordered_map<node_id, std::size_t> index_;
  node_id focused_;
  node_id modal_;
};

// Reads the semantics of every visible widget in `source`.
[[nodiscard]] auto build(widget_tree const &source) -> tree;

// What changed between two snapshots, for incremental platform updates.
struct tree_diff {
  std::vector<node_id> added;
  std::vector<node_id> removed;
  // Present in both and different, including a changed child list.
  std::vector<node_id> changed;
  bool focus_changed = false;

  [[nodiscard]] auto empty() const -> bool {
    return added.empty() && removed.empty() && changed.empty() && !focus_changed;
  }
};

[[nodiscard]] auto diff(tree const &before, tree const &after) -> tree_diff;

// Asks the widget behind `id` to perform `request`, as an assistive
// technology would, through `widget::perform_accessibility_action`. False when
// the widget is gone, hidden, blocked by a modal, not exposing the action, or
// declines it.
auto perform_action(widget_tree &target, node_id id, action_request const &request) -> bool;

enum class issue_kind : std::uint8_t {
  // A role that requires a name has none.
  missing_name,
  // An interactive widget cannot take keyboard focus, so a keyboard
  // user cannot reach it.
  interactive_not_focusable,
  // An interactive widget has no action besides focus.
  missing_actions,
  // An interactive widget has no area.
  empty_bounds,
  // `labelled_by` or `described_by` refers to a widget that is not in the tree.
  dangling_reference,
};

struct issue {
  node_id id;
  issue_kind kind;

  [[nodiscard]] friend constexpr auto operator==(issue const &, issue const &) -> bool = default;
};

// Checks a snapshot against the Cubit accessibility requirements that can be
// decided from semantics alone.
[[nodiscard]] auto audit(tree const &snapshot) -> std::vector<issue>;

} // namespace basalt::accessibility
