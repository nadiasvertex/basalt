#pragma once

#include "basalt/units.h"
#include "basalt/widget.h"
#include "basalt/widgets/grid_layout.h"
#include "basalt/widgets/scroll_view.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace basalt {

class collection_body;

// A scrolling grid for collections of any size. The application says how many
// items there are and how big a cell is. The grid works out the number of
// columns from its width and creates widgets only for the rows that are in
// view, plus a margin of rows on each side. A cell that leaves the margin is
// kept in a pool and handed back to the factory for the next index that needs
// one, so scrolling through thousands of items reuses a few dozen widgets.
//
// Items belong to sections. A grid made with `set_item_count` has one section.
// A section with a header shows a full-width header widget above its cells.
//
// The cell factory builds or updates a cell. It receives the item index and a
// widget the grid is done with, or null when none is available. It returns the
// widget to show: it may update and return `recycled`, or return a new widget.
// The grid sets the bounds of what it gets back.
//
// The grid reports the range of items that are in view, without the margin.
// The application uses it to start the loads that cells need and to cancel
// loads for items that scrolled away. The handler runs during layout, so it
// must not add or remove widgets of this grid.
//
// A vertical scroll bar shows over the right edge when the content overflows.
// The grid is a Tab stop. It keeps a current item, which the arrow keys, Home,
// End, Page Up and Page Down move. Enter and Space activate the current item,
// and so does a click on a cell that does not handle the click itself.
class collection_view : public widget {
public:
  using cell_factory =
      std::function<std::unique_ptr<widget>(std::size_t index, std::unique_ptr<widget> recycled)>;
  using header_factory = std::function<std::unique_ptr<widget>(std::size_t section)>;

  explicit collection_view(std::string name = {});
  ~collection_view() override;

  // One section without a header.
  auto set_item_count(std::size_t count) -> void;
  auto set_sections(std::vector<grid_section> sections) -> void;
  [[nodiscard]] auto item_count() const -> std::size_t { return item_count_; }
  [[nodiscard]] auto sections() const -> std::vector<grid_section> const & { return sections_; }

  // The size of every cell, and the space between cells and rows.
  auto set_cell_size(length width, length height) -> void;
  auto set_gap(length gap) -> void;
  auto set_header_height(length height) -> void;
  // The rows kept ready beyond the view on each side. The default is two.
  auto set_margin_rows(std::size_t rows) -> void;

  auto set_cell_factory(cell_factory factory) -> void;
  auto set_header_factory(header_factory factory) -> void;
  // Runs the cell and header factories again for every item in view. Call it
  // when the data behind the cells changed but the counts did not.
  auto reload() -> void;

  // The items in view, without the margin, as of the last layout.
  [[nodiscard]] auto visible_items() const -> index_range { return visible_; }
  // The items that have a widget, including the margin.
  [[nodiscard]] auto realized_items() const -> index_range { return realized_; }
  // The cell for `index`, or null when it has no widget now.
  [[nodiscard]] auto cell(std::size_t index) const -> widget *;
  // Called when `visible_items()` changes.
  auto set_on_visible_change(std::function<void(index_range)> handler) -> void {
    on_visible_change_ = std::move(handler);
  }

  // Scrolls as little as possible to show the item. Works before the first
  // layout; the grid scrolls when it knows its width.
  auto scroll_to_item(std::size_t index) -> void;
  [[nodiscard]] auto scroll_offset() const -> float;
  auto set_scroll_offset(float offset) -> void;

  // The item that keys move and activate, if any. Setting it scrolls it into
  // view and does not call a handler.
  [[nodiscard]] auto current() const -> std::optional<std::size_t> { return current_; }
  auto set_current(std::optional<std::size_t> index) -> void;
  // Called with the new current item when the user moves it.
  auto set_on_current_change(std::function<void(std::size_t)> handler) -> void {
    on_current_change_ = std::move(handler);
  }
  // Called with the item the user activates.
  auto set_on_activate(std::function<void(std::size_t)> handler) -> void {
    on_activate_ = std::move(handler);
  }

  [[nodiscard]] auto name() const -> std::string const & { return name_; }
  auto set_name(std::string name) -> void { name_ = std::move(name); }

  // The geometry for the current width, cell size and sections.
  [[nodiscard]] auto layout() const -> grid_layout const &;

  [[nodiscard]] auto focus_ring() const -> std::optional<focus_ring_shape> override;
  [[nodiscard]] auto accessibility_info() const -> accessibility::info override;

protected:
  auto on_event(event_context &context, event const &e) -> bool override;
  auto on_layout() -> void override;
  [[nodiscard]] auto on_measure(constraints const &limits) const -> basalt::size override;
  auto on_environment_changed() -> void override;

private:
  friend class collection_body;

  [[nodiscard]] auto layout_for(float width) const -> grid_layout const &;
  auto geometry_changed() -> void;
  auto move_current(std::optional<std::size_t> target) -> bool;
  auto activate(std::size_t index) -> void;
  auto handle_key(key code) -> bool;
  auto report_visible(index_range visible) -> void;

  std::string name_;
  std::vector<grid_section> sections_;
  std::size_t item_count_ = 0;
  length cell_width_ = fixed(160);
  length cell_height_ = fixed(160);
  length gap_ = fixed(8);
  length header_height_ = fixed(32);
  std::size_t margin_rows_ = 2;
  cell_factory cell_factory_;
  header_factory header_factory_;
  std::function<void(index_range)> on_visible_change_;
  std::function<void(std::size_t)> on_current_change_;
  std::function<void(std::size_t)> on_activate_;

  std::optional<std::size_t> current_;
  std::optional<std::size_t> pending_reveal_;
  index_range visible_;
  index_range realized_;
  mutable grid_layout layout_;
  mutable float layout_width_ = -1;
  mutable bool layout_stale_ = true;
  scroll_view *scroller_ = nullptr;
  collection_body *body_ = nullptr;
};

} // namespace basalt
