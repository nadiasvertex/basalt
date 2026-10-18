#pragma once

#include "basalt/widget.h"
#include "basalt/widgets/scroll_view.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace basalt {

class tree_body;
class tree_item;

// One row of a tree as the application shows it.
struct tree_row {
  std::string text;
  // Secondary text, shown subdued at the end of the row.
  std::string detail;
  // 0 for a top-level row.
  std::uint32_t depth = 0;
  bool expandable = false;
  bool expanded = false;
  // The row's 1-based position among its siblings and their count, for assistive technology,
  // which cannot count rows that are not realized. 0 when unknown.
  std::uint32_t position_in_set = 0;
  std::uint32_t set_size = 0;

  [[nodiscard]] friend auto operator==(tree_row const &, tree_row const &) -> bool = default;
};

// The rows of a tree, flattened in display order. A collapsed row's descendants are not rows.
// The tree view asks only for the rows it shows, so the application keeps the data and the tree
// view holds no copy of it.
struct tree_rows {
  std::function<std::size_t()> count;
  std::function<tree_row(std::size_t)> row;
  // Optional. A row's depth without its text, for the walks over a subtree or to a parent that
  // the keyboard and an expand or collapse make. Without it, they read whole rows.
  std::function<std::uint32_t(std::size_t)> depth;
};

// A tree of rows that scrolls vertically. It realizes widgets only for the rows in view and for
// the row that keeps keyboard focus, so its cost does not grow with the number of rows.
//
// The application owns the expanded state. A click on a chevron, Right, Left or an assistive
// technology's expand and collapse call the expand handler; the handler changes the data and
// calls `rows_changed`. The selection follows the expanded row's subtree across that change.
//
// Single selection follows keyboard focus. The rows form one Tab stop. Up, Down, Page Up, Page
// Down, Home and End move the selection; Right expands a row or moves to its first child; Left
// collapses a row or moves to its parent; typed text moves to the next row whose text starts
// with it. A click selects a row. Enter and a double click activate it.
//
// The tree has no natural width; its layout gives it one.
class tree_view : public widget {
public:
  explicit tree_view(std::string name = "Tree");

  // Replaces the rows and reads them.
  auto set_rows(tree_rows rows) -> void;
  // Reads the rows again after the data changed. The selection is kept by index, clamped to the
  // rows, except across an expand or collapse.
  auto rows_changed() -> void;
  [[nodiscard]] auto row_count() const -> std::size_t { return count_; }

  [[nodiscard]] auto selected() const -> std::optional<std::size_t> { return selected_; }
  // Selects a row, or nothing, without calling the select handler. Focus moves with the selection
  // when it is in the tree, and is not taken otherwise. The view does not scroll.
  auto select(std::optional<std::size_t> index) -> void;
  // Scrolls as little as possible to show the row.
  auto scroll_to_row(std::size_t index) -> void;

  // The height of every row, in layout pixels.
  [[nodiscard]] auto row_height() const -> float;
  // Where the row is, in this widget's coordinates, as of the last layout.
  [[nodiscard]] auto row_area(std::size_t index) const -> rect;
  // The widget showing a row, or null when the row is not realized.
  [[nodiscard]] auto realized_row(std::size_t index) const -> widget const *;
  [[nodiscard]] auto realized_row_count() const -> std::size_t;

  // Called when the user selects a row.
  auto set_on_select(std::function<void(std::size_t)> handler) -> void {
    on_select_ = std::move(handler);
  }
  // Called when the user activates a row.
  auto set_on_activate(std::function<void(std::size_t)> handler) -> void {
    on_activate_ = std::move(handler);
  }
  // Called with the row and whether the user asked to expand it or collapse it.
  auto set_on_expand(std::function<void(std::size_t, bool)> handler) -> void {
    on_expand_ = std::move(handler);
  }

  [[nodiscard]] auto accessibility_info() const -> accessibility::info override;

protected:
  auto on_event(event_context &context, event const &e) -> bool override;
  auto on_layout() -> void override;
  [[nodiscard]] auto on_measure(constraints const &limits) const -> basalt::size override;

private:
  friend class tree_body;
  friend class tree_item;

  // An expand or collapse whose effect on the selection waits for `rows_changed`.
  struct pending_toggle {
    std::size_t index = 0;
    // One past the toggled row's last descendant, before the change.
    std::size_t subtree_end = 0;
    std::size_t count = 0;
  };

  [[nodiscard]] auto row(std::size_t index) const -> tree_row;
  [[nodiscard]] auto depth_of(std::size_t index) const -> std::uint32_t;
  // The row that holds the Tab stop: the selected row, or the first.
  [[nodiscard]] auto anchor() const -> std::size_t { return selected_.value_or(0); }
  [[nodiscard]] auto parent_of(std::size_t index) const -> std::optional<std::size_t>;
  [[nodiscard]] auto subtree_end(std::size_t index) const -> std::size_t;
  [[nodiscard]] auto find_typed(std::string const &prefix, std::size_t from) const
      -> std::optional<std::size_t>;
  [[nodiscard]] auto rows_per_page() const -> std::size_t;
  // Selects `index`, moves focus to it when focus is in the tree and scrolls it into view.
  auto move_to(std::size_t index, focus_reason reason) -> void;
  auto place_selection(std::optional<std::size_t> index, focus_reason reason) -> void;
  auto user_select(std::size_t index, focus_reason reason) -> void;
  auto activate(std::size_t index) -> void;
  // A primary press on a row, on its chevron or not, and whether it is the second of a double
  // click.
  auto press(std::size_t index, bool on_mark, bool twice) -> void;
  auto toggle(std::size_t index) -> void;
  auto type_ahead(text_input_event const &input) -> bool;

  std::string name_;
  tree_rows rows_;
  std::size_t count_ = 0;
  std::optional<std::size_t> selected_;
  std::optional<pending_toggle> pending_;
  std::string typed_;
  event_time typed_at_{};
  scroll_view *scroller_ = nullptr;
  tree_body *body_ = nullptr;
  std::function<void(std::size_t)> on_select_;
  std::function<void(std::size_t)> on_activate_;
  std::function<void(std::size_t, bool)> on_expand_;
};

} // namespace basalt
