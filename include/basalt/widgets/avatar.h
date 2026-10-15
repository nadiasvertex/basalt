#pragma once

#include "basalt/image.h"
#include "basalt/widget.h"
#include "basalt/widgets/control_size.h"
#include "basalt/widgets/text_line.h"
#include "basalt/widgets/tooltip.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>

namespace basalt {

enum class avatar_layout : std::uint8_t {
  // The text follows the graphic.
  trailing,
  // The text comes before the graphic.
  leading,
  // Only the graphic; the name is shown in a tooltip.
  graphic_only,
};

// The initials of a name: the first letter of the first and the last word,
// upper-cased when they are ASCII. A single word gives one letter and an empty
// name gives none.
[[nodiscard]] auto avatar_initials(std::string_view name) -> std::string;

// A circular graphic that stands for a person or entity: a photo when there
// is one, otherwise initials from the name. The name and supporting text can
// sit beside it. Photos are drawn as the square the graphic occupies.
class avatar : public widget {
public:
  explicit avatar(std::string name = {}, control_size size = control_size::medium);

  [[nodiscard]] auto name() const -> std::string const & { return name_.text(); }
  auto set_name(std::string name) -> void;
  [[nodiscard]] auto supporting_text() const -> std::string const & { return supporting_.text(); }
  auto set_supporting_text(std::string text) -> void;
  [[nodiscard]] auto image() const -> raster_image_ptr const & { return image_; }
  auto set_image(raster_image_ptr image) -> void;
  [[nodiscard]] auto layout() const -> avatar_layout { return layout_; }
  auto set_layout(avatar_layout layout) -> void;
  [[nodiscard]] auto size() const -> control_size { return size_; }
  auto set_size(control_size size) -> void;

  // The graphic's diameter in layout pixels.
  [[nodiscard]] auto diameter() const -> float;

  [[nodiscard]] auto accessibility_info() const -> accessibility::info override;

protected:
  auto on_paint(render_context &context) const -> void override;
  [[nodiscard]] auto on_measure(constraints const &limits) const -> basalt::size override;
  auto on_interaction_changed(interaction_state const &previous) -> void override;
  auto on_animation_frame(std::chrono::nanoseconds elapsed) -> bool override;
  auto on_detached() -> void override;

private:
  auto refresh_tooltip() -> void;

  text_line name_;
  text_line supporting_;
  text_line initials_;
  raster_image_ptr image_;
  avatar_layout layout_ = avatar_layout::trailing;
  control_size size_;
  tooltip_anchor tip_;
};

} // namespace basalt
