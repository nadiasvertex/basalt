#pragma once

#include "basalt/widget.h"
#include "basalt/widgets/button.h"
#include "basalt/widgets/control_size.h"
#include "basalt/widgets/text_line.h"
#include "basalt/widgets/tooltip.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace basalt {

class chip_group;

// A compact pill with a label. A chip can be selectable (it toggles on
// activation), removable (it has a remove mark that calls the remove handler,
// as do Delete and Backspace), or both. Activation is a primary press released
// over the chip, or Space or Enter. A loading chip is busy and ignores
// activation. When `max_width` cuts the label short, a tooltip shows it whole.
class chip : public widget {
public:
  explicit chip(std::string label = {}, control_size size = control_size::medium);

  [[nodiscard]] auto label() const -> std::string const & { return line_.text(); }
  auto set_label(std::string label) -> void;
  [[nodiscard]] auto size() const -> control_size { return size_; }
  auto set_size(control_size size) -> void;

  [[nodiscard]] auto selectable() const -> bool { return selectable_; }
  auto set_selectable(bool selectable) -> void;
  [[nodiscard]] auto selected() const -> bool { return selected_; }
  // Does not call the handler and does not apply a group's selection mode.
  auto set_selected(bool selected) -> void;
  [[nodiscard]] auto removable() const -> bool { return removable_; }
  auto set_removable(bool removable) -> void;
  [[nodiscard]] auto has_error() const -> bool { return error_; }
  auto set_error(bool error) -> void;
  [[nodiscard]] auto loading() const -> bool { return loading_; }
  auto set_loading(bool loading) -> void;
  // The widest the chip grows; unbounded when unset.
  [[nodiscard]] auto max_width() const -> std::optional<length> { return max_width_; }
  auto set_max_width(std::optional<length> width) -> void;

  // Called with the new state after the user changes the selection.
  auto set_on_toggle(std::function<void(bool)> handler) -> void { on_toggle_ = std::move(handler); }
  // Called when the user removes the chip. Inside a `chip_group` the group also
  // removes the chip afterwards.
  auto set_on_remove(std::function<void()> handler) -> void { on_remove_ = std::move(handler); }

  // Do what a click does. False when the chip is disabled, loading or does
  // not support the action.
  auto toggle() -> bool;
  auto remove() -> bool;

  [[nodiscard]] auto accessibility_info() const -> accessibility::info override;

protected:
  auto on_event(event_context &context, event const &e) -> bool override;
  auto on_paint(render_context &context) const -> void override;
  [[nodiscard]] auto on_measure(constraints const &limits) const -> basalt::size override;
  auto on_interaction_changed(interaction_state const &previous) -> void override;
  auto on_animation_frame(std::chrono::nanoseconds elapsed) -> bool override;
  auto on_detached() -> void override;
  auto on_accessibility_action(accessibility::action_request const &request) -> bool override;

private:
  friend class chip_group;

  struct layout_parts;
  [[nodiscard]] auto parts() const -> layout_parts;
  [[nodiscard]] auto can_act() const -> bool;
  auto refresh_tooltip() -> void;

  text_line line_;
  control_size size_;
  std::optional<length> max_width_;
  bool selectable_ = false;
  bool selected_ = false;
  bool removable_ = false;
  bool error_ = false;
  bool loading_ = false;
  std::function<void(bool)> on_toggle_;
  std::function<void()> on_remove_;
  tooltip_anchor tooltip_;
};

enum class chip_selection : std::uint8_t { none, single, multiple };

// Chips in rows that wrap at the group's width. With `single` selection the
// group deselects the other chips; with `multiple` it leaves them alone.
// Removed chips leave the group. An optional Remove All button follows the
// chips while there are removable ones.
class chip_group : public widget {
public:
  explicit chip_group(std::string name = {}, chip_selection selection = chip_selection::none);

  // Adds a chip and returns it.
  auto add_chip(std::string label) -> chip &;
  [[nodiscard]] auto chip_count() const -> std::size_t;
  // Precondition: `index < chip_count()`.
  [[nodiscard]] auto chip_at(std::size_t index) const -> chip &;
  [[nodiscard]] auto selected_indices() const -> std::vector<std::size_t>;

  [[nodiscard]] auto selection() const -> chip_selection { return selection_; }
  auto set_selection(chip_selection selection) -> void;
  // Shows a button with this label after the chips; empty hides it.
  auto set_remove_all_label(std::string label) -> void;
  auto remove_all() -> void;
  // Called with the chip's index after the user changes the selection.
  auto set_on_change(std::function<void(std::size_t)> handler) -> void {
    on_change_ = std::move(handler);
  }

  [[nodiscard]] auto accessibility_info() const -> accessibility::info override;

protected:
  auto on_layout() -> void override;
  [[nodiscard]] auto on_measure(constraints const &limits) const -> basalt::size override;
  auto on_child_removed(widget &child) -> void override;

private:
  friend class chip;

  [[nodiscard]] auto flow(float max_width, bool place) const -> basalt::size;
  auto chip_toggled(chip &source) -> void;
  auto chip_removed(chip &source) -> void;
  auto refresh_remove_all() -> void;
  [[nodiscard]] auto remove_all_button() const -> button *;

  std::string name_;
  chip_selection selection_;
  std::string remove_all_label_;
  widget_id remove_all_id_;
  std::function<void(std::size_t)> on_change_;
};

} // namespace basalt
