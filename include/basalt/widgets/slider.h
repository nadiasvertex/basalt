#pragma once

#include "basalt/widget.h"
#include "basalt/widgets/control_size.h"
#include "basalt/widgets/text_line.h"

#include <functional>
#include <string>

namespace basalt {

// A horizontal slider that picks a number from a range. The value always lies
// on a multiple of the step, counted from the minimum, and inside the range.
// An optional label shows above the track on the left, and optional value text
// shows above it on the right.
//
// Two handlers separate live feedback from final choices. `on_change` runs each
// time the value changes while the user drags or presses a key, so an owner can
// update a preview. `on_commit` runs once when the choice is final: when a drag
// ends, and after each keyboard change or click on the track.
//
// Pressing the track moves the thumb to that point and starts a drag. Left and
// Down decrease the value by one step. Right and Up increase it. Page Down and
// Page Up move by a tenth of the range, and Home and End go to the ends.
// Escape during a drag restores the value from before the drag.
class slider : public widget {
public:
  explicit slider(float minimum = 0, float maximum = 1, float step = 0.01f, float value = 0,
                  control_size size = control_size::medium);

  [[nodiscard]] auto minimum() const -> float { return minimum_; }
  [[nodiscard]] auto maximum() const -> float { return maximum_; }
  [[nodiscard]] auto step() const -> float { return step_; }
  // A maximum below the minimum becomes the minimum. A step of zero or less
  // becomes one thousandth of the range. The value is adjusted to fit.
  auto set_range(float minimum, float maximum, float step) -> void;

  [[nodiscard]] auto value() const -> float { return value_; }
  // Rounds to a step and clamps. Does not call a handler.
  auto set_value(float value) -> void;

  auto set_label(std::string label) -> void;
  [[nodiscard]] auto label() const -> std::string const & { return label_.text(); }
  // Formats the text that shows at the right of the label row. Without a
  // formatter the row shows no value.
  auto set_value_formatter(std::function<std::string(float)> formatter) -> void;

  auto set_on_change(std::function<void(float)> handler) -> void {
    on_change_ = std::move(handler);
  }
  auto set_on_commit(std::function<void(float)> handler) -> void {
    on_commit_ = std::move(handler);
  }

  [[nodiscard]] auto dragging() const -> bool { return dragging_; }
  // The thumb's center along the track, in this widget's coordinates.
  [[nodiscard]] auto thumb_center() const -> point;
  // The track's rectangle in this widget's coordinates.
  [[nodiscard]] auto track_rect() const -> rect;

  [[nodiscard]] auto accessibility_info() const -> accessibility::info override;

protected:
  auto on_event(event_context &context, event const &e) -> bool override;
  auto on_paint(render_context &context) const -> void override;
  [[nodiscard]] auto on_measure(constraints const &limits) const -> basalt::size override;
  auto on_accessibility_action(accessibility::action_request const &request) -> bool override;

private:
  struct metrics;
  [[nodiscard]] auto measure_parts() const -> metrics;
  [[nodiscard]] auto snap(float value) const -> float;
  [[nodiscard]] auto value_at(float x) const -> float;
  [[nodiscard]] auto value_text() const -> std::string;
  // Applies a user change: updates the value, calls `on_change` when it moved
  // and `on_commit` when `final` is set.
  auto apply(float value, bool final) -> void;
  auto handle_key(key code, modifier modifiers) -> bool;

  float minimum_;
  float maximum_;
  float step_;
  float value_;
  text_line label_;
  mutable text_line value_line_;
  std::function<std::string(float)> formatter_;
  std::function<void(float)> on_change_;
  std::function<void(float)> on_commit_;
  bool dragging_ = false;
  float drag_start_value_ = 0;
};

} // namespace basalt
