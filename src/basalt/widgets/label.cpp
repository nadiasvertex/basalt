#include "basalt/widgets/label.h"

#include "basalt/widget_tree.h"

#include <utility>

namespace basalt {

auto text_color_for(theme const &t, text_tone tone) -> rgba {
  switch (tone) {
  case text_tone::normal:
    break;
  case text_tone::subdued:
    return t.colors.text_subdued;
  case text_tone::interactive:
    return t.colors.text_interactive;
  case text_tone::danger:
    return t.colors.text_danger;
  case text_tone::warning:
    return t.colors.text_warning;
  case text_tone::success:
    return t.colors.text_success;
  }
  return t.colors.text_default;
}

label::label(std::string text, text_role role) : line_(std::move(text), role) {}

auto label::set_text(std::string text) -> void {
  if (line_.set_text(std::move(text))) {
    invalidate_content();
  }
}

auto label::set_role(text_role role) -> void {
  if (line_.set_role(role)) {
    invalidate_content();
  }
}

auto label::set_tone(text_tone tone) -> void {
  if (tone != tone_) {
    tone_ = tone;
    invalidate_paint();
  }
}

auto label::set_heading_level(std::uint32_t level) -> void { heading_level_ = level; }

auto label::accessibility_info() const -> accessibility::info {
  accessibility::info details;
  details.kind = heading_level_ > 0 ? accessibility::role::heading : accessibility::role::label;
  details.name = line_.text();
  details.level = heading_level_;
  return details;
}

auto label::on_measure(constraints const &) const -> basalt::size { return line_.measure(*this); }

auto label::on_paint(render_context &context) const -> void {
  if (tree() == nullptr) {
    return;
  }
  line_.draw_in(context, *this, {{}, bounds().extent}, text_color_for(theme(), tone_));
}

} // namespace basalt
