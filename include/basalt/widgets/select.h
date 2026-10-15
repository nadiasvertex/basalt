#pragma once

#include "basalt/widget.h"
#include "basalt/widgets/control_size.h"
#include "basalt/widgets/field_text.h"
#include "basalt/widgets/text_line.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace basalt {

class menu;

enum class select_mode : std::uint8_t { single, multiple };

// A field that shows the chosen value, or a placeholder, and opens a list of
// options in a popup (see `overlay_host`). It needs an `overlay_host` above it
// to open.
//
// With `select_mode::single`, choosing an option closes the list. With
// `select_mode::multiple`, the field shows how many options are chosen with a
// button that clears them, the list stays open while options are toggled, and
// chosen options come first the next time it opens. Enter, Space and Down
// open the list; in the multiple mode Delete and Backspace clear the choice.
// A label sits above the field and helper text, or the error message, below.
class select : public widget {
public:
  explicit select(std::string label = {}, control_size size = control_size::medium);
  ~select() override;

  [[nodiscard]] auto label() const -> std::string const & { return field_.label().text(); }
  auto set_label(std::string label) -> void;
  [[nodiscard]] auto helper_text() const -> std::string const & { return field_.helper().text(); }
  // In the error state it holds the error message.
  auto set_helper_text(std::string text) -> void;
  [[nodiscard]] auto placeholder() const -> std::string const & {
    return field_.placeholder().text();
  }
  auto set_placeholder(std::string text) -> void;
  [[nodiscard]] auto has_error() const -> bool { return field_.has_error(); }
  auto set_error(bool error) -> void;
  [[nodiscard]] auto size() const -> control_size { return field_.size(); }
  auto set_size(control_size size) -> void;
  [[nodiscard]] auto mode() const -> select_mode { return mode_; }
  // Keeps the first chosen option when changing to `single`.
  auto set_mode(select_mode mode) -> void;

  auto add_option(std::string label) -> void;
  [[nodiscard]] auto option_count() const -> std::size_t { return options_.size(); }
  // Precondition: `index < option_count()`.
  [[nodiscard]] auto option_label(std::size_t index) const -> std::string const &;
  [[nodiscard]] auto is_chosen(std::size_t index) const -> bool;
  // Chosen options in the order they were added.
  [[nodiscard]] auto chosen_indices() const -> std::vector<std::size_t>;
  // Sets or clears the choice without calling the change handler. In the
  // single mode choosing replaces the previous choice. False when `index` is
  // out of range.
  auto choose(std::size_t index, bool chosen = true) -> bool;
  auto clear() -> void;
  // Called after the user changes the choice.
  auto set_on_change(std::function<void()> handler) -> void { on_change_ = std::move(handler); }

  [[nodiscard]] auto is_open() const -> bool;
  // False when disabled, when there are no options or when there is no host.
  auto open() -> bool;
  auto close() -> void;

  [[nodiscard]] auto accessibility_info() const -> accessibility::info override;

protected:
  auto on_event(event_context &context, event const &e) -> bool override;
  auto on_paint(render_context &context) const -> void override;
  [[nodiscard]] auto on_measure(constraints const &limits) const -> basalt::size override;
  auto on_detached() -> void override;
  auto on_accessibility_action(accessibility::action_request const &request) -> bool override;

private:
  struct layout_parts;
  [[nodiscard]] auto parts() const -> layout_parts;
  [[nodiscard]] auto value_text() const -> std::string;
  auto option_activated(std::size_t index) -> void;
  auto changed() -> void;

  std::vector<std::string> options_;
  std::vector<bool> chosen_;
  field_text field_;
  select_mode mode_ = select_mode::single;
  widget_id popup_;
  menu *list_ = nullptr;
  std::function<void()> on_change_;
};

} // namespace basalt
