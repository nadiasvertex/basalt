#pragma once

#include "basalt/widget.h"
#include "basalt/widget_id.h"
#include "basalt/widgets/control_size.h"
#include "basalt/widgets/field_text.h"

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace basalt {

class text_field;

// A single-line text field with a list of suggestions in a popup (see `overlay_host`). The writer
// can pick a suggestion or type any text.
//
// Suggestions belong to a named section. The list shows the suggestions that contain the typed
// text, ignoring case, under a heading for each section that has a match. A suggestion that
// appears in more than one section is listed once, in the first. The list opens when the writer
// types, presses Down or presses the chevron, and closes on Escape, on Enter, when a suggestion
// is chosen and when focus leaves the field. Down and Up move through the suggestions while the
// list is open, and Enter takes the highlighted one. The list is a hover popup, so focus stays in
// the text field while it is open and a press outside it that does not move focus leaves it
// open.
//
// Choosing a suggestion replaces the text with it. The submit handler runs with the text when the
// writer presses Enter or chooses a suggestion. The change handler runs after every edit that the
// writer types, not after a choice or `set_text`.
class combo_box : public widget {
public:
  explicit combo_box(std::string label = {}, control_size size = control_size::medium);
  ~combo_box() override;

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

  [[nodiscard]] auto text() const -> std::string const &;
  // Replaces the text without calling a handler.
  auto set_text(std::string text) -> void;

  // An empty section name lists the suggestion without a heading.
  auto add_suggestion(std::string section, std::string text) -> void;
  auto clear_suggestions() -> void;
  [[nodiscard]] auto suggestion_count() const -> std::size_t { return suggestions_.size(); }
  // The suggestions the list shows for the current text, in display order.
  [[nodiscard]] auto matching_suggestions() const -> std::vector<std::string>;
  // The suggestion that Enter would take, or none.
  [[nodiscard]] auto highlighted_suggestion() const -> std::optional<std::string>;

  // Called with the text after every edit that the writer types.
  auto set_on_change(std::function<void(std::string const &)> handler) -> void {
    on_change_ = std::move(handler);
  }
  // Called with the text on Enter and when the writer chooses a suggestion.
  auto set_on_submit(std::function<void(std::string const &)> handler) -> void {
    on_submit_ = std::move(handler);
  }

  [[nodiscard]] auto is_open() const -> bool;
  // False when disabled, when nothing matches or when there is no host.
  auto open() -> bool;
  auto close() -> void;

  // The text field that takes the writer's input, for focus and tests.
  [[nodiscard]] auto input() -> text_field & { return *input_; }

  [[nodiscard]] auto accessibility_info() const -> accessibility::info override;

protected:
  auto on_event(event_context &context, event const &e) -> bool override;
  auto on_paint(render_context &context) const -> void override;
  auto on_layout() -> void override;
  [[nodiscard]] auto on_measure(constraints const &limits) const -> basalt::size override;
  auto on_interaction_changed(interaction_state const &previous) -> void override;
  auto on_detached() -> void override;
  auto on_accessibility_action(accessibility::action_request const &request) -> bool override;

private:
  struct suggestion {
    std::string section;
    std::string text;
  };
  struct layout_parts;
  [[nodiscard]] auto parts() const -> layout_parts;
  // The suggestions that contain the text, grouped by section in display order.
  [[nodiscard]] auto matches() const -> std::vector<suggestion>;
  auto typed() -> void;
  auto submit(std::string text) -> void;
  auto choose(std::string text) -> void;
  auto move_highlight(int direction) -> void;
  // Brings the open list in step with the text. Closes it when nothing matches.
  auto refresh_list() -> void;
  auto dismiss_popup() -> void;

  std::vector<suggestion> suggestions_;
  field_text field_;
  text_field *input_ = nullptr;
  widget_id popup_;
  widget *list_ = nullptr;
  std::optional<std::size_t> highlighted_;
  std::function<void(std::string const &)> on_change_;
  std::function<void(std::string const &)> on_submit_;
};

} // namespace basalt
