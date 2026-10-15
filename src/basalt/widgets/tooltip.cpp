#include "basalt/widgets/tooltip.h"

#include "basalt/render_context.h"
#include "basalt/widget_tree.h"
#include "basalt/widgets/overlay_host.h"

#include <memory>
#include <utility>

namespace basalt {

namespace {

constexpr text_role tooltip_role = text_role::small;

} // namespace

tooltip::tooltip(std::string text) : line_(std::move(text), tooltip_role) {
  set_hit_testable(false);
}

auto tooltip::set_text(std::string text) -> void {
  if (line_.set_text(std::move(text))) {
    invalidate_content();
  }
}

auto tooltip::accessibility_info() const -> accessibility::info {
  accessibility::info details;
  details.kind = accessibility::role::tooltip;
  details.name = line_.text();
  return details;
}

auto tooltip::on_measure(constraints const &) const -> basalt::size {
  basalt::theme const &t = theme();
  basalt::size const text = line_.measure(*this);
  return {text.width + 2 * resolve(t.sizes.inset_horizontal_tooltip),
          text.height + 2 * resolve(t.sizes.inset_vertical_tooltip)};
}

auto tooltip::on_paint(render_context &context) const -> void {
  if (tree() == nullptr) {
    return;
  }
  basalt::theme const &t = theme();
  rect const area{{}, bounds().extent};
  context.fill_rounded_rect(area, resolve(t.sizes.radius_default),
                            t.colors.background_default_dark);
  line_.draw_in(context, *this, area, t.colors.text_default_on_dark, alignment::center,
                alignment::center);
}

auto tooltip_anchor::set_text(widget &owner, std::string text) -> void {
  text_ = std::move(text);
  if (text_.empty()) {
    delay_.cancel();
    hide(owner);
  }
}

auto tooltip_anchor::update(widget &owner) -> bool {
  interaction_state const &s = owner.interaction();
  bool const wanted = !text_.empty() && (s.hovered || s.focus_visible);
  if (!wanted) {
    delay_.cancel();
    hide(owner);
    return false;
  }
  if (visible() || delay_.pending()) {
    return false;
  }
  delay_.start();
  return true;
}

auto tooltip_anchor::advance(widget &owner, std::chrono::nanoseconds elapsed) -> bool {
  if (delay_.advance(elapsed)) {
    show(owner);
  }
  return delay_.pending();
}

auto tooltip_anchor::show(widget &owner) -> void {
  overlay_host *const host = overlay_host::find(owner);
  if (host == nullptr || visible() || text_.empty()) {
    return;
  }
  rect const anchor = owner.bounds_in_root();
  rect const placed{host->from_root(anchor.origin), anchor.extent};
  shown_ =
      host->show_hover_popup(std::make_unique<tooltip>(text_), placed, [this] { shown_ = {}; });
}

auto tooltip_anchor::hide(widget &owner) -> void {
  if (!visible()) {
    return;
  }
  if (overlay_host *const host = overlay_host::find(owner)) {
    host->dismiss(std::exchange(shown_, {}));
  }
  shown_ = {};
}

} // namespace basalt
