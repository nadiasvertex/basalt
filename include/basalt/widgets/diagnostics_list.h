#pragma once

#include "basalt/widgets/box.h"
#include "basalt/widgets/text_line.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace basalt {

enum class diagnostic_severity : std::uint8_t { error, warning, info };

// One problem to show. `location` is whatever the application calls the
// place, such as "word/document.xml:12:5"; it is shown but not interpreted.
struct diagnostic_entry {
  diagnostic_severity severity = diagnostic_severity::info;
  std::string message;
  std::string location;

  [[nodiscard]] friend auto operator==(diagnostic_entry const &, diagnostic_entry const &)
      -> bool = default;
};

// A list of problems, one row each, with an icon whose shape (not only its
// color) tells errors, warnings and notes apart. The rows form one focus
// group: Tab enters the list, and Up, Down, Home and End move among the rows.
// Clicking a row, or pressing Enter or Space on it, calls the activation
// handler with the row's index so the application can go to the location.
//
// The list is as tall as its rows; put it in a `scroll_view` to scroll it.
class diagnostics_list : public box {
public:
  explicit diagnostics_list(std::string name = "Diagnostics");

  [[nodiscard]] auto entries() const -> std::vector<diagnostic_entry> const & { return entries_; }
  // Replaces every row. Focus returns to the list's first row if it was in
  // the list.
  auto set_entries(std::vector<diagnostic_entry> entries) -> void;

  // The row with keyboard focus, or empty.
  [[nodiscard]] auto focused_index() const -> std::optional<std::size_t>;
  // Moves keyboard focus to the row. False when `index` is out of range.
  auto focus_row(std::size_t index) -> bool;

  // Shown when there are no entries.
  [[nodiscard]] auto empty_message() const -> std::string const & { return empty_.text(); }
  auto set_empty_message(std::string message) -> void;

  // Called with the row index and its entry.
  auto set_on_activate(std::function<void(std::size_t, diagnostic_entry const &)> handler) -> void {
    on_activate_ = std::move(handler);
  }

  [[nodiscard]] auto accessibility_info() const -> accessibility::info override;

protected:
  auto on_event(event_context &context, event const &e) -> bool override;
  auto on_paint(render_context &context) const -> void override;
  [[nodiscard]] auto on_measure(constraints const &limits) const -> basalt::size override;

private:
  friend class diagnostic_row;

  auto row_activated(std::size_t index) -> void;

  std::string name_;
  std::vector<diagnostic_entry> entries_;
  text_line empty_;
  std::function<void(std::size_t, diagnostic_entry const &)> on_activate_;
};

} // namespace basalt
