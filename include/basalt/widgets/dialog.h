#pragma once

#include "basalt/widgets/box.h"
#include "basalt/widgets/button.h"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace basalt {

struct dialog_button {
  std::string label;
  button_variant variant = button_variant::secondary;
};

// A message with a title and a row of buttons, for `overlay_host::show_dialog`.
// The message wraps at about `wrap_columns` characters and keeps its line
// breaks. Choosing a button, or pressing Escape (which chooses the button at
// `cancel_index`), calls the choice handler with the button's index; closing
// the dialog is up to the owner. Focus starts on the first button.
class dialog : public box {
public:
  static constexpr std::size_t wrap_columns = 64;

  dialog(std::string title, std::string message, std::vector<dialog_button> buttons,
         std::size_t cancel_index = static_cast<std::size_t>(-1));

  auto set_on_choice(std::function<void(std::size_t)> handler) -> void {
    on_choice_ = std::move(handler);
  }
  [[nodiscard]] auto title() const -> std::string const & { return title_; }
  [[nodiscard]] auto button_count() const -> std::size_t { return button_count_; }
  // Presses the button as a click would. False when `index` is out of range.
  auto choose(std::size_t index) -> bool;

  [[nodiscard]] auto accessibility_info() const -> accessibility::info override;

protected:
  auto on_event(event_context &context, event const &e) -> bool override;
  auto on_paint(render_context &context) const -> void override;

private:
  std::string title_;
  std::string message_;
  std::size_t button_count_ = 0;
  std::size_t cancel_index_;
  std::function<void(std::size_t)> on_choice_;
};

// Breaks `text` into lines of at most `columns` characters at spaces, keeping
// existing line breaks.
[[nodiscard]] auto wrap_message(std::string const &text, std::size_t columns)
    -> std::vector<std::string>;

} // namespace basalt
