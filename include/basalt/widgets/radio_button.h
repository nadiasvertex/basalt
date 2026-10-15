#pragma once

#include "basalt/widgets/box.h"
#include "basalt/widgets/choice_control.h"

#include <cstddef>
#include <functional>
#include <optional>
#include <string>

namespace basalt {

class radio_group;

// One option of a `radio_group`. Activating it selects it and deselects the
// others. Inside a group, the arrow keys move focus to the neighbouring
// option and select it.
class radio_button : public choice_control {
public:
  explicit radio_button(std::string label = {}, control_size size = control_size::medium);

  [[nodiscard]] auto selected() const -> bool { return selected_; }
  // Does not deselect other options; use `radio_group::select`.
  auto set_selected(bool selected) -> void;

protected:
  auto perform_activation() -> void override;
  [[nodiscard]] auto indicator_size(basalt::theme const &t) const -> basalt::size override;
  auto paint_indicator(render_context &context, rect box, basalt::theme const &t,
                       control_state state) const -> void override;
  auto describe(accessibility::info &details) const -> void override;
  auto on_event(event_context &context, event const &e) -> bool override;

private:
  bool selected_ = false;
};

// A column of radio buttons that are mutually exclusive. The group is one Tab
// stop: Tab enters it at the selected option, and the arrow keys move among
// the options.
class radio_group : public box {
public:
  explicit radio_group(std::string name = {}, axis direction = axis::vertical);

  // Adds an option and returns it.
  auto add_option(std::string label) -> radio_button &;

  [[nodiscard]] auto option_count() const -> std::size_t { return child_count(); }
  // Precondition: `index < option_count()`.
  [[nodiscard]] auto option(std::size_t index) const -> radio_button &;
  [[nodiscard]] auto selected_index() const -> std::optional<std::size_t>;
  // Selects the option and deselects the rest without calling the change
  // handler. False when `index` is out of range.
  auto select(std::size_t index) -> bool;
  // Called with the option's index after the user changes the selection.
  auto set_on_change(std::function<void(std::size_t)> handler) -> void {
    on_change_ = std::move(handler);
  }

  [[nodiscard]] auto accessibility_info() const -> accessibility::info override;

private:
  friend class radio_button;

  auto choose(radio_button const &button) -> void;

  std::string name_;
  std::function<void(std::size_t)> on_change_;
};

} // namespace basalt
