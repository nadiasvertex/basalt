#pragma once

#include "basalt/widgets/box.h"
#include "basalt/widgets/text_field.h"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace basalt {

// One row of a palette. `detail` is shown on the right, for example key bindings. `category` is
// muted text after the label, and `state` is muted text before the detail, such as "On". A
// dimmed entry is drawn in muted colors but can still be chosen; the owner decides what that does.
struct palette_entry {
  std::string id;
  std::string label;
  std::string detail;
  std::string category;
  std::string state;
  bool dimmed = false;

  [[nodiscard]] friend auto operator==(palette_entry const &, palette_entry const &)
      -> bool = default;
};

// A search box above a short list of choices, for `overlay_host::show_dialog`.
//
// The palette does not filter. It reports each change of the query and the owner answers with
// `set_entries`, already ranked. Keyboard focus stays in the search box: Up, Down, PageUp and
// PageDown move the selection, and Enter chooses the selected entry. Clicking a row chooses it.
// Escape is handled by the overlay. At most `visible_rows` entries are drawn; the list scrolls
// to keep the selection in view.
//
// The palette does not close itself. The choice handler runs first, then the owner dismisses.
class palette : public box {
public:
  static constexpr std::size_t visible_rows = 10;

  explicit palette(std::string name = "Command palette");

  [[nodiscard]] auto name() const -> std::string const & { return name_; }
  [[nodiscard]] auto query() const -> std::string const & { return field_->text(); }
  // Does not call the query handler.
  auto set_query(std::string query) -> void { field_->set_text(std::move(query)); }
  auto set_placeholder(std::string placeholder) -> void {
    field_->set_placeholder(std::move(placeholder));
  }

  [[nodiscard]] auto entries() const -> std::vector<palette_entry> const & { return entries_; }
  // Replaces the list and selects the first entry.
  auto set_entries(std::vector<palette_entry> entries) -> void;
  // Shown instead of rows when there are no entries.
  auto set_empty_message(std::string message) -> void;

  [[nodiscard]] auto selected_index() const -> std::size_t { return selected_; }
  // False when `index` is out of range.
  auto select(std::size_t index) -> bool;
  auto move_selection(int delta) -> void;
  // Calls the choice handler with the selected entry's id. False when the list is empty.
  auto choose_selected() -> bool;

  // Called with the new query after every edit.
  auto set_on_query_change(std::function<void(std::string const &)> handler) -> void {
    on_query_change_ = std::move(handler);
  }
  // Called with the id of the chosen entry.
  auto set_on_choose(std::function<void(std::string const &)> handler) -> void {
    on_choose_ = std::move(handler);
  }

  auto focus_query() -> void { field_->request_focus(); }

  [[nodiscard]] auto accessibility_info() const -> accessibility::info override;

protected:
  auto on_event(event_context &context, event const &e) -> bool override;
  auto on_paint(render_context &context) const -> void override;

private:
  friend class palette_row;

  auto rebuild_rows() -> void;
  auto row_chosen(std::size_t entry_index) -> void;

  std::string name_;
  text_field *field_ = nullptr;
  box *rows_ = nullptr;
  std::vector<palette_entry> entries_;
  std::string empty_message_ = "No matches";
  std::size_t selected_ = 0;
  std::size_t first_visible_ = 0;
  std::function<void(std::string const &)> on_query_change_;
  std::function<void(std::string const &)> on_choose_;
};

} // namespace basalt
