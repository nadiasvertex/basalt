#pragma once

#include "basalt/accessibility/node.h"
#include "basalt/widgets/box.h"
#include "basalt/widgets/choice_control.h"

#include <cstddef>
#include <functional>
#include <string>

namespace basalt {

// A checkbox with the values unchecked, checked and mixed. Activating a mixed
// checkbox checks it. The error state is set by the owner.
class checkbox : public choice_control {
public:
  explicit checkbox(std::string label = {},
                    accessibility::check_state state = accessibility::check_state::unchecked,
                    control_size size = control_size::medium);

  [[nodiscard]] auto state() const -> accessibility::check_state { return state_; }
  // Does not call the change handler.
  auto set_state(accessibility::check_state state) -> void;
  [[nodiscard]] auto checked() const -> bool {
    return state_ == accessibility::check_state::checked;
  }
  // Called with the new value after the user changes it.
  auto set_on_change(std::function<void(accessibility::check_state)> handler) -> void {
    on_change_ = std::move(handler);
  }

protected:
  auto perform_activation() -> void override;
  [[nodiscard]] auto indicator_size(basalt::theme const &t) const -> basalt::size override;
  auto paint_indicator(render_context &context, rect box, basalt::theme const &t,
                       control_state state) const -> void override;
  auto describe(accessibility::info &details) const -> void override;

private:
  accessibility::check_state state_;
  std::function<void(accessibility::check_state)> on_change_;
};

// A vertical list of checkboxes. A group with a "select all" checkbox shows
// it first: checking it checks every option, unchecking it unchecks them, and
// it is mixed while only some options are checked.
class checkbox_group : public box {
public:
  explicit checkbox_group(std::string name = {});

  // Adds an option and returns it.
  auto add_option(std::string label, bool checked = false) -> checkbox &;
  // Shows a checkbox that checks and unchecks every option. Call it before
  // adding options.
  auto enable_select_all(std::string label) -> void;

  [[nodiscard]] auto option_count() const -> std::size_t;
  // Precondition: `index < option_count()`.
  [[nodiscard]] auto option(std::size_t index) const -> checkbox &;
  [[nodiscard]] auto checked_count() const -> std::size_t;
  // Called with the option's index after the user changes an option.
  auto set_on_change(std::function<void(std::size_t)> handler) -> void {
    on_change_ = std::move(handler);
  }

  [[nodiscard]] auto accessibility_info() const -> accessibility::info override;

private:
  auto first_option_child() const -> std::size_t { return select_all_ ? 1 : 0; }
  auto refresh_select_all() -> void;
  auto set_all(bool checked) -> void;

  std::string name_;
  checkbox *select_all_ = nullptr;
  std::function<void(std::size_t)> on_change_;
};

} // namespace basalt
