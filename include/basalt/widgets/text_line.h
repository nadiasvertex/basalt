#pragma once

#include "basalt/color.h"
#include "basalt/geometry.h"
#include "basalt/layout.h"
#include "basalt/render_context.h"
#include "basalt/text/text_shaper.h"
#include "basalt/theme.h"
#include "basalt/typography.h"
#include "basalt/widget.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace basalt {

// Which theme text style a piece of text uses.
enum class text_role : std::uint8_t {
  body,
  body_strong,
  small,
  small_strong,
  large,
  heading_small,
  heading_medium,
  heading_large,
  // Long-form article text.
  reading,
  // Archive and XML source in the monospace family.
  source,
};

[[nodiscard]] auto text_style_for(theme const &t, text_role role) -> text_style;

// One line of text that remembers its shaping. Widgets own one per visible
// string. The shaped result is kept until the text, role, theme, scale or
// the tree's shaper changes. Without a shaper the line has no width and a
// height of one line, so layout stays well-defined.
class text_line {
public:
  text_line() = default;
  explicit text_line(std::string text, text_role role = text_role::body)
      : text_(std::move(text)), role_(role) {}

  [[nodiscard]] auto text() const -> std::string const & { return text_; }
  // False when the text did not change.
  auto set_text(std::string text) -> bool;
  [[nodiscard]] auto role() const -> text_role { return role_; }
  auto set_role(text_role role) -> bool;

  // The natural size in layout pixels: the shaped advance by the line height.
  [[nodiscard]] auto measure(widget const &owner) const -> basalt::size;

  // Draws with the top left of the line box at `origin`.
  auto draw(render_context &context, widget const &owner, point origin, rgba color) const -> void;
  // Draws inside `area`, aligned along each axis; `stretch` behaves as `start`.
  auto draw_in(render_context &context, widget const &owner, rect area, rgba color,
               alignment horizontal = alignment::start,
               alignment vertical = alignment::center) const -> void;

private:
  struct cache {
    text::text_shaper const *shaper = nullptr;
    text_style style;
    std::string text;
    bool valid = false;
    text::shaped_text shaped;
    float line_height = 0;
  };

  auto shaped(widget const &owner) const -> cache const &;

  std::string text_;
  text_role role_ = text_role::body;
  mutable cache cache_;
};

} // namespace basalt
