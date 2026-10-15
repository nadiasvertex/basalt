#include "basalt/widgets/popover.h"

#include "basalt/event_router.h"
#include "basalt/widget_tree.h"
#include "basalt/widgets/label.h"
#include "basalt/widgets/overlay_host.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <variant>

namespace basalt {

namespace {

// The panel that the host shows: a background, a border, the content and a
// tip on the side that faces the trigger.
class popover_panel : public widget {
public:
  popover_panel(std::unique_ptr<widget> content, bool tip) : tip_(tip) {
    add_child(std::move(content));
    set_focus_scope(focus_scope::group);
  }

  auto place(bool below, float anchor_center) -> void {
    below_ = below;
    anchor_center_ = anchor_center;
  }

protected:
  [[nodiscard]] auto on_measure(constraints const &limits) const -> basalt::size override {
    basalt::theme const &t = theme();
    float const min_width = resolve(t.sizes.dimension_width_popover_min);
    float const max_width =
        std::min(resolve(t.sizes.dimension_width_popover_max), limits.max.width);
    insets const pad = padding(t);
    basalt::size const inner = child(0).measure(
        constraints::loose({std::max(0.0f, max_width - pad.horizontal()), unbounded}));
    return {std::clamp(inner.width + pad.horizontal(), std::min(min_width, max_width), max_width),
            inner.height + pad.vertical() + (tip_ ? tip_height(t) : 0)};
  }

  auto on_layout() -> void override {
    basalt::theme const &t = theme();
    insets const pad = padding(t);
    float const top = pad.top + (tip_ && below_ ? tip_height(t) : 0);
    child(0).set_bounds(
        {{pad.left, top},
         {std::max(0.0f, bounds().width() - pad.horizontal()),
          std::max(0.0f, bounds().height() - pad.vertical() - (tip_ ? tip_height(t) : 0))}});
  }

  auto on_paint(render_context &context) const -> void override {
    if (tree() == nullptr) {
      return;
    }
    basalt::theme const &t = theme();
    float const tip = tip_ ? tip_height(t) : 0;
    rect const body{{0, tip_ && below_ ? tip : 0}, {bounds().width(), bounds().height() - tip}};
    float const radius = resolve(t.sizes.radius_default);
    float const stroke = device_stroke_width(fixed(1), scale());
    context.fill_rounded_rect(body, radius, t.colors.background_default);
    context.stroke_rounded_rect(inset(body, insets::uniform(stroke / 2)), radius,
                                t.colors.border_default, stroke);
    if (tip_) {
      paint_tip(context, t, body, tip, stroke);
    }
  }

private:
  [[nodiscard]] auto padding(basalt::theme const &t) const -> insets {
    return insets::symmetric(resolve(t.sizes.inset_horizontal_panel),
                             resolve(t.sizes.inset_vertical_panel));
  }
  [[nodiscard]] auto tip_height(basalt::theme const &t) const -> float {
    return resolve(t.sizes.dimension_icon) / 2;
  }

  // A triangle built from strips, which every renderer can draw. The strip
  // that meets the body is open, so the border does not close the tip.
  auto paint_tip(render_context &context, basalt::theme const &t, rect body, float tip,
                 float stroke) const -> void {
    float const radius = resolve(t.sizes.radius_default);
    float const center = std::clamp(anchor_center_ - bounds_in_root().left(), radius + tip,
                                    std::max(radius + tip, bounds().width() - radius - tip));
    int const rows = std::max(1, static_cast<int>(std::ceil(tip / stroke)));
    float const row_height = tip / static_cast<float>(rows);
    for (int i = 0; i < rows; ++i) {
      float const extent = 2 * tip * static_cast<float>(i + 1) / static_cast<float>(rows);
      float const y = below_ ? body.top() - tip + static_cast<float>(i) * row_height
                             : body.bottom() + tip - static_cast<float>(i + 1) * row_height;
      rect const strip{{center - extent / 2, y}, {extent, row_height}};
      if (i == rows - 1) {
        context.fill_rect(strip, t.colors.background_default);
        continue;
      }
      context.fill_rect(strip, t.colors.border_default);
      context.fill_rect(inset(strip, insets::symmetric(stroke, 0)), t.colors.background_default);
    }
  }

  bool tip_;
  bool below_ = true;
  float anchor_center_ = 0;
};

} // namespace

popover::popover(popover_trigger trigger) : trigger_(trigger) {}

popover::~popover() = default;

auto popover::replace_trigger(std::unique_ptr<widget> trigger) -> void {
  while (child_count() > 0) {
    (void)remove_child(child(0));
  }
  add_child(std::move(trigger));
  invalidate_layout();
}

auto popover::set_trigger_kind(popover_trigger trigger) -> void {
  if (trigger != trigger_) {
    close();
    delay_.cancel();
    trigger_ = trigger;
  }
}

auto popover::set_content(std::function<std::unique_ptr<widget>()> factory) -> void {
  content_ = std::move(factory);
}

auto popover::set_text(std::string text) -> void {
  set_content([text = std::move(text)] { return std::make_unique<label>(text, text_role::small); });
}

auto popover::is_open() const -> bool {
  overlay_host const *const host = overlay_host::find(*this);
  return host != nullptr && panel_.valid() && host->is_open(panel_);
}

auto popover::open() -> bool {
  overlay_host *const host = overlay_host::find(*this);
  if (host == nullptr || !content_ || !enabled_in_tree() || is_open()) {
    return false;
  }
  auto content = content_();
  if (content == nullptr) {
    return false;
  }

  rect const root_anchor = bounds_in_root();
  rect const anchor{host->from_root(root_anchor.origin), root_anchor.extent};
  auto shown = std::make_unique<popover_panel>(std::move(content), tip_);
  // The host places the panel below the anchor when it fits and above it
  // otherwise; the tip goes on the side that faces the anchor.
  basalt::size const wanted = shown->measure(constraints::loose(host->bounds().extent));
  shown->place(anchor.bottom() + wanted.height <= host->bounds().height(),
               root_anchor.left() + root_anchor.width() / 2);
  auto const closed = [this] { panel_closed(); };
  panel_ = trigger_ == popover_trigger::hover
               ? host->show_hover_popup(std::move(shown), anchor, closed)
               : host->show_popup(std::move(shown), anchor, closed);
  host->make_exclusive(panel_);
  if (on_visibility_) {
    auto const handler = on_visibility_;
    handler(true);
  }
  return true;
}

auto popover::panel_closed() -> void {
  bool const was_open = panel_.valid();
  panel_ = {};
  if (was_open && on_visibility_) {
    auto const handler = on_visibility_;
    handler(false);
  }
}

auto popover::close() -> void {
  delay_.cancel();
  if (overlay_host *const host = overlay_host::find(*this); host != nullptr && panel_.valid()) {
    host->dismiss(panel_);
  }
}

auto popover::toggle() -> bool {
  if (is_open()) {
    close();
    return true;
  }
  return open();
}

auto popover::on_detached() -> void {
  // The owner is going away too, so it is not told.
  auto handler = std::exchange(on_visibility_, {});
  close();
  on_visibility_ = std::move(handler);
}

auto popover::on_interaction_changed(interaction_state const &previous) -> void {
  widget::on_interaction_changed(previous);
  if (trigger_ != popover_trigger::hover) {
    return;
  }
  interaction_state const &s = interaction();
  if (s.hovered || s.focus_within) {
    if (!is_open() && !delay_.pending()) {
      delay_.start();
      request_animation_frame();
    }
  } else {
    delay_.cancel();
    close();
  }
}

auto popover::on_animation_frame(std::chrono::nanoseconds elapsed) -> bool {
  if (delay_.advance(elapsed)) {
    open();
  }
  return delay_.pending();
}

auto popover::on_event(event_context &, event const &e) -> bool {
  if (trigger_ != popover_trigger::click) {
    return false;
  }
  if (auto const *p = std::get_if<pointer_event>(&e)) {
    if (p->button != pointer_button::primary) {
      return false;
    }
    if (p->action == pointer_action::down) {
      return true;
    }
    if (p->action == pointer_action::up) {
      if (interaction().pressed) {
        toggle();
      }
      return true;
    }
    return false;
  }
  if (auto const *k = std::get_if<key_event>(&e)) {
    if (k->pressed && !k->repeat && k->modifiers == modifier::none &&
        (k->code == key::enter || k->code == key::space)) {
      return toggle();
    }
  }
  return false;
}

auto popover::on_measure(constraints const &limits) const -> basalt::size {
  return child_count() > 0 ? child(0).measure(limits) : limits.min;
}

auto popover::on_layout() -> void {
  if (child_count() > 0) {
    child(0).set_bounds({{}, bounds().extent});
  }
}

} // namespace basalt
