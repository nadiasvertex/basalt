#pragma once

#include "basalt/color.h"
#include "basalt/theme.h"
#include "basalt/widget.h"
#include "basalt/widgets/text_line.h"

#include <cstdint>
#include <string>

namespace basalt {

// The semantic color of text, chosen from the theme.
enum class text_tone : std::uint8_t { normal, subdued, interactive, danger, warning, success };

[[nodiscard]] auto text_color_for(theme const &t, text_tone tone) -> rgba;

// A single line of non-interactive text. It is clipped to its bounds, so a
// parent that wants to show all of it gives it its measured width.
class label : public widget {
public:
  explicit label(std::string text = {}, text_role role = text_role::body);

  [[nodiscard]] auto text() const -> std::string const & { return line_.text(); }
  auto set_text(std::string text) -> void;
  [[nodiscard]] auto role() const -> text_role { return line_.role(); }
  auto set_role(text_role role) -> void;
  [[nodiscard]] auto tone() const -> text_tone { return tone_; }
  auto set_tone(text_tone tone) -> void;

  // Nonzero marks the label as a heading of that level for assistive
  // technology; it does not change the appearance.
  [[nodiscard]] auto heading_level() const -> std::uint32_t { return heading_level_; }
  auto set_heading_level(std::uint32_t level) -> void;

  [[nodiscard]] auto accessibility_info() const -> accessibility::info override;

protected:
  auto on_paint(render_context &context) const -> void override;
  [[nodiscard]] auto on_measure(constraints const &limits) const -> basalt::size override;

private:
  text_line line_;
  text_tone tone_ = text_tone::normal;
  std::uint32_t heading_level_ = 0;
};

} // namespace basalt
