#include "basalt/widgets/tabs.h"

#include "basalt/event_router.h"
#include "basalt/widget_tree.h"
#include "basalt/widgets/menu.h"
#include "basalt/widgets/overlay_host.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string_view>
#include <utility>
#include <variant>

#include "glyphs.h"
#include "option_list.h"
#include "press_activation.h"

namespace basalt {

namespace {

// Byte offset after the first `count` code points of `text`.
auto prefix_end(std::string_view text, std::size_t count) -> std::size_t {
  std::size_t at = 0;
  while (count > 0 && at < text.size()) {
    ++at;
    while (at < text.size() && (static_cast<unsigned char>(text[at]) & 0xC0) == 0x80) {
      ++at;
    }
    --count;
  }
  return at;
}

// Byte offset where the last `count` code points of `text` begin.
auto suffix_begin(std::string_view text, std::size_t count) -> std::size_t {
  std::size_t at = text.size();
  while (count > 0 && at > 0) {
    --at;
    while (at > 0 && (static_cast<unsigned char>(text[at]) & 0xC0) == 0x80) {
      --at;
    }
    --count;
  }
  return at;
}

auto code_point_count(std::string_view text) -> std::size_t {
  std::size_t count = 0;
  for (char const c : text) {
    if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) {
      ++count;
    }
  }
  return count;
}

// Layout pixels at the default scale. Tabs shrink to the minimum before the row scrolls.
constexpr float min_tab_width = 96;
constexpr float max_tab_width = 280;
// Part of the neighbouring tab left visible when a tab is scrolled into view.
constexpr float reveal_margin = 32;
// Pixels moved by one wheel notch.
constexpr float wheel_step = 48;
// Width of each overflow control and of the fade beside them.
constexpr float control_width = 28;
constexpr float fade_width = 24;
constexpr int fade_strips = 6;
// A tab this close to fully shown does not count as partly hidden.
constexpr float hidden_tolerance = 1;
// The tab menu scrolls beyond this many rows.
constexpr std::size_t menu_rows = 10;
constexpr float menu_min_width = 200;
// A held scroll button repeats after the delay, then at the interval.
constexpr std::chrono::milliseconds repeat_delay{400};
constexpr std::chrono::milliseconds repeat_interval{80};
// Space around and between the tabs of a segmented bar.
constexpr float segment_spacing = 2;

// The offset that shows [left, right] with `margin` to spare, moving as little as possible from
// `offset`. A tab too wide to show with its margins is left alone.
auto revealed(float left, float right, float offset, float view, float margin) -> float {
  if (right - left + 2 * margin >= view) {
    return offset;
  }
  if (left - margin < offset) {
    return std::max(0.0f, left - margin);
  }
  if (right + margin > offset + view) {
    return right + margin - view;
  }
  return offset;
}

enum class overflow_glyph : std::uint8_t { back, forward, menu };

} // namespace

// A control at an end of a scrolling `tab_bar`: a scroll button or the button that opens the
// menu of tabs. Scroll buttons act on press and repeat while held; the menu button acts on
// release like a button.
class overflow_button final : public widget {
public:
  overflow_button(overflow_glyph glyph, std::string name, std::function<void()> on_press)
      : glyph_(glyph), name_(std::move(name)), on_press_(std::move(on_press)) {
    // A pointer press focuses the control, but Tab skips it.
    set_focus_policy(focus_policy::click);
  }

  auto set_background(rgba tokens::color_set::*color) -> void {
    background_ = color;
    invalidate_paint();
  }
  auto set_expanded(bool expanded) -> void {
    if (expanded != expanded_) {
      expanded_ = expanded;
      invalidate_paint();
    }
  }

  [[nodiscard]] auto accessibility_info() const -> accessibility::info override {
    accessibility::info details;
    details.kind = accessibility::role::button;
    details.name = name_;
    details.actions = {accessibility::action::press, accessibility::action::focus};
    if (glyph_ == overflow_glyph::menu) {
      details.expanded = expanded_;
    }
    return details;
  }

protected:
  auto on_event(event_context &, event const &e) -> bool override {
    if (glyph_ == overflow_glyph::menu) {
      return handle_press_activation(e, interaction(), {}, [this] { on_press_(); });
    }
    auto const *p = std::get_if<pointer_event>(&e);
    if (p == nullptr || p->button != pointer_button::primary) {
      return false;
    }
    if (p->action == pointer_action::down) {
      held_ = {};
      next_repeat_ = repeat_delay;
      on_press_();
      request_animation_frame();
      return true;
    }
    return p->action == pointer_action::up;
  }

  auto on_animation_frame(std::chrono::nanoseconds elapsed) -> bool override {
    if (!interaction().pressed || !enabled()) {
      return false;
    }
    held_ += elapsed;
    if (held_ >= next_repeat_) {
      next_repeat_ = held_ + repeat_interval;
      on_press_();
    }
    return true;
  }

  auto on_accessibility_action(accessibility::action_request const &request) -> bool override {
    if (request.kind != accessibility::action::press) {
      return false;
    }
    on_press_();
    return true;
  }

  [[nodiscard]] auto on_measure(constraints const &) const -> basalt::size override {
    return {resolve(fixed(control_width)), 0};
  }

  auto on_paint(render_context &context) const -> void override {
    if (tree() == nullptr) {
      return;
    }
    basalt::theme const &t = theme();
    float const hairline = device_stroke_width(fixed(1), scale());
    rect const area{{}, bounds().extent};
    context.fill_rect({area.origin, {area.width(), std::max(0.0f, area.height() - hairline)}},
                      t.colors.*background_);
    rect const target = inset(area, insets::uniform(resolve(fixed(2))));
    switch (control_state_of(interaction(), enabled())) {
    case control_state::hovered:
      context.fill_rounded_rect(target, resolve(t.sizes.radius_default),
                                t.colors.background_accent_transparent_hover);
      break;
    case control_state::pressed:
      context.fill_rounded_rect(target, resolve(t.sizes.radius_default),
                                t.colors.background_accent_transparent_active);
      break;
    case control_state::normal:
    case control_state::disabled:
      break;
    }
    float const side = resolve(t.sizes.dimension_icon);
    rect const icon{{(area.width() - side) / 2, (area.height() - side) / 2}, {side, side}};
    rgba const color = enabled() ? t.colors.icon_default
                                 : with_opacity(t.colors.icon_default, opacity_from_percent(40));
    float const stroke = device_stroke_width(fixed(1.5f), scale());
    switch (glyph_) {
    case overflow_glyph::back:
      glyphs::draw_chevron_sideways(context, icon, false, color, stroke);
      break;
    case overflow_glyph::forward:
      glyphs::draw_chevron_sideways(context, icon, true, color, stroke);
      break;
    case overflow_glyph::menu:
      glyphs::draw_chevron(context, icon, !expanded_, color, stroke);
      break;
    }
  }

private:
  overflow_glyph glyph_;
  std::string name_;
  std::function<void()> on_press_;
  rgba tokens::color_set::*background_ = &tokens::color_set::background_default;
  bool expanded_ = false;
  std::chrono::nanoseconds held_{};
  std::chrono::nanoseconds next_repeat_{};
};

namespace {

constexpr std::size_t back_control = 0;
constexpr std::size_t forward_control = 1;
constexpr std::size_t menu_control = 2;

} // namespace

tab::tab(std::string label) : line_(std::move(label), text_role::body_strong) {
  set_focus_policy(focus_policy::strong);
  set_minimum_target(true);
}

auto tab::set_label(std::string label) -> void {
  if (line_.set_text(std::move(label))) {
    invalidate_content();
  }
}

auto tab::set_selected(bool selected) -> void {
  if (selected != selected_) {
    selected_ = selected;
    invalidate_paint();
  }
}

auto tab::request_selection() -> void {
  if (auto *const bar = parent_as<tab_bar>()) {
    if (auto const index = bar->index_of(*this)) {
      bar->select(*index);
    }
  }
}

auto tab::set_closable(bool closable) -> void {
  if (closable != closable_) {
    closable_ = closable;
    invalidate_content();
  }
}

auto tab::request_close() -> void {
  if (auto *const bar = parent_as<tab_bar>()) {
    if (auto const index = bar->index_of(*this)) {
      auto const handler = bar->on_close_;
      if (handler) {
        // The handler may destroy this tab; do not touch members afterwards.
        handler(*index);
      }
    }
  }
}

auto tab::close_area() const -> rect {
  auto const &sizes = theme().sizes;
  float const side = resolve(fixed(16));
  float const edge = resolve(sizes.inset_horizontal_tab);
  return {{bounds().width() - edge - side, (bounds().height() - side) / 2}, {side, side}};
}

auto tab::over_close(point local) const -> bool {
  if (!closable_) {
    return false;
  }
  return close_area().contains(local);
}

auto tab::fitted_label(float available) const -> text_line const & {
  float const natural = line_.measure(*this).width;
  if (natural <= available) {
    return line_;
  }
  if (fitted_width_ == available && fitted_natural_ == natural && fitted_source_ == line_.text()) {
    return fitted_line_;
  }
  fitted_width_ = available;
  fitted_natural_ = natural;
  fitted_source_ = line_.text();
  std::string_view const full = fitted_source_;
  auto const build = [&](std::size_t kept) {
    std::size_t const head = (kept + 1) / 2;
    std::size_t const tail = kept / 2;
    std::string out{full.substr(0, prefix_end(full, head))};
    out += "\u2026";
    out += full.substr(suffix_begin(full, tail));
    return out;
  };
  fitted_line_.set_role(line_.role());
  // Find the most code points that still fit alongside the ellipsis.
  std::size_t low = 0;
  std::size_t high = code_point_count(full);
  while (low < high) {
    std::size_t const mid = (low + high + 1) / 2;
    fitted_line_.set_text(build(mid));
    if (fitted_line_.measure(*this).width <= available) {
      low = mid;
    } else {
      high = mid - 1;
    }
  }
  fitted_line_.set_text(build(low));
  return fitted_line_;
}

auto tab::accessibility_info() const -> accessibility::info {
  accessibility::info details;
  details.kind = accessibility::role::tab;
  details.name = line_.text();
  details.selected = selected_;
  details.actions = {accessibility::action::press, accessibility::action::select,
                     accessibility::action::focus};
  if (auto const *const bar = parent_as<tab_bar>()) {
    if (auto const index = bar->index_of(*this)) {
      details.position_in_set = static_cast<std::uint32_t>(*index + 1);
      details.set_size = static_cast<std::uint32_t>(bar->tab_count());
    }
  }
  return details;
}

auto tab::on_accessibility_action(accessibility::action_request const &request) -> bool {
  if (request.kind == accessibility::action::press ||
      request.kind == accessibility::action::select) {
    request_selection();
    return true;
  }
  return false;
}

auto tab::on_interaction_changed(interaction_state const &previous) -> void {
  widget::on_interaction_changed(previous);
  if (interaction().focused && !previous.focused) {
    request_selection();
  }
}

auto tab::on_event(event_context &, event const &e) -> bool {
  if (auto const *p = std::get_if<pointer_event>(&e)) {
    if (closable_) {
      bool const over = p->action != pointer_action::leave && over_close(p->position);
      if (over != close_hovered_) {
        close_hovered_ = over;
        invalidate_paint();
      }
    }
    if (p->action == pointer_action::down && p->button == pointer_button::primary &&
        over_close(p->position)) {
      close_pressed_ = true;
      return true;
    }
    if (p->action == pointer_action::up && p->button == pointer_button::primary && close_pressed_) {
      close_pressed_ = false;
      if (over_close(p->position)) {
        request_close();
      }
      return true;
    }
  }
  return handle_press_activation(e, interaction(), {}, [this] { request_selection(); });
}

auto tab::segmented() const -> bool {
  auto const *const bar = parent_as<tab_bar>();
  return bar != nullptr && bar->style() == tab_bar_style::segmented;
}

auto tab::on_measure(constraints const &) const -> basalt::size {
  auto const &sizes = theme().sizes;
  if (segmented()) {
    return {line_.measure(*this).width + 2 * resolve(sizes.inset_horizontal_button),
            resolve(sizes.dimension_height_element_min)};
  }
  float const close = closable_ ? resolve(fixed(16)) + resolve(fixed(4)) : 0.0f;
  return {line_.measure(*this).width + 2 * resolve(sizes.inset_horizontal_tab) + close,
          resolve(sizes.dimension_height_tab)};
}

auto tab::on_paint(render_context &context) const -> void {
  if (tree() == nullptr) {
    return;
  }
  basalt::theme const &t = theme();
  rect const area{{}, bounds().extent};
  if (segmented()) {
    float const radius = resolve(t.sizes.radius_default);
    if (selected_) {
      context.fill_rounded_rect(area, radius, t.colors.background_accent_subdued);
    } else if (auto const state = control_state_of(interaction(), enabled());
               state == control_state::hovered) {
      context.fill_rounded_rect(area, radius, t.colors.background_neutral_hover);
    } else if (state == control_state::pressed) {
      context.fill_rounded_rect(area, radius, t.colors.background_neutral_active);
    }
    float const inset_x = resolve(t.sizes.inset_horizontal_button);
    rect const text_area{{inset_x, 0}, {std::max(0.0f, area.width() - 2 * inset_x), area.height()}};
    fitted_label(text_area.width())
        .draw_in(context, *this, text_area,
                 selected_ ? t.colors.text_interactive : t.colors.text_subdued, alignment::center);
    return;
  }
  switch (control_state_of(interaction(), enabled())) {
  case control_state::hovered:
    context.fill_rect(area, t.colors.background_accent_transparent_hover);
    break;
  case control_state::pressed:
    context.fill_rect(area, t.colors.background_accent_transparent_active);
    break;
  case control_state::normal:
  case control_state::disabled:
    break;
  }
  rect text_area = area;
  float const inset = resolve(t.sizes.inset_horizontal_tab);
  text_area.origin.x += inset;
  text_area.extent.width = std::max(0.0f, text_area.extent.width - 2 * inset);
  if (closable_) {
    rect const close = close_area();
    text_area.extent.width = std::max(0.0f, close.left() - resolve(fixed(4)) - text_area.origin.x);
    if (close_hovered_) {
      context.fill_rounded_rect(close, resolve(t.sizes.radius_default),
                                t.colors.background_accent_transparent_active);
    }
    float const arm = close.width() * 0.25f;
    point const middle = close.center();
    float const width = device_stroke_width(fixed(1.5f), scale());
    rgba const color = close_hovered_ ? t.colors.text_default : t.colors.icon_default;
    context.line(middle - point{arm, arm}, middle + point{arm, arm}, color, width);
    context.line(middle - point{arm, -arm}, middle + point{arm, -arm}, color, width);
  }
  fitted_label(text_area.width())
      .draw_in(context, *this, text_area, selected_ ? t.colors.text_default : t.colors.text_subdued,
               alignment::center);
  if (selected_) {
    float const height = device_stroke_width(fixed(2), scale());
    context.fill_rect({{0, area.height() - height}, {area.width(), height}},
                      t.colors.border_accent);
  }
}

tab_bar::tab_bar(std::string name) : box(axis::horizontal), name_(std::move(name)) {
  set_focus_scope(focus_scope::group);
  set_cross_alignment(alignment::stretch);
  ensure_controls();
}

auto tab_bar::tab_count() const -> std::size_t {
  auto const controls = static_cast<std::size_t>(
      std::ranges::count_if(controls_, [](overflow_button const *c) { return c != nullptr; }));
  return child_count() - controls;
}

auto tab_bar::tab_item() const -> box_item {
  if (style_ == tab_bar_style::segmented) {
    return {.grow = 1, .basis = fixed(0)};
  }
  return {.min = fixed(min_tab_width), .max = fixed(max_tab_width)};
}

auto tab_bar::ensure_controls() -> void {
  if (controls_[back_control] != nullptr) {
    return;
  }
  auto const add_control = [this](std::size_t slot, overflow_glyph glyph, std::string label,
                                  std::function<void()> action) {
    auto &added = emplace_child<overflow_button>(glyph, std::move(label), std::move(action));
    added.set_background(background_);
    added.set_visible(false);
    // The controls take no space in the row; `place_controls` positions them.
    set_item(added, {.shrink = 0, .basis = fixed(0), .max = fixed(0)});
    controls_[slot] = &added;
  };
  add_control(back_control, overflow_glyph::back, "Scroll tabs left",
              [this] { scroll_toward_start(); });
  add_control(forward_control, overflow_glyph::forward, "Scroll tabs right",
              [this] { scroll_toward_end(); });
  add_control(menu_control, overflow_glyph::menu, "All tabs", [this] {
    if (tab_menu_open()) {
      close_tab_menu();
    } else {
      open_tab_menu();
    }
  });
}

auto tab_bar::on_child_removed(widget &child) -> void {
  box::on_child_removed(child);
  for (auto &control : controls_) {
    if (control == &child) {
      control = nullptr;
    }
  }
}

auto tab_bar::on_attached() -> void {
  box::on_attached();
  // Whether the menu button shows depends on an overlay host above the bar.
  invalidate_layout();
}

auto tab_bar::add_tab(std::string label) -> tab & {
  ensure_controls();
  auto &added = insert_child(tab_count(), std::make_unique<tab>(std::move(label)));
  set_item(added, tab_item());
  if (tab_count() == 1) {
    select(0);
  }
  return added;
}

auto tab_bar::set_style(tab_bar_style style) -> void {
  if (style == style_) {
    return;
  }
  style_ = style;
  bool const segmented = style_ == tab_bar_style::segmented;
  set_padding(segmented ? length_insets::uniform(fixed(segment_spacing)) : length_insets{});
  set_gap(segmented ? fixed(segment_spacing) : length{});
  for (std::size_t i = 0; i < tab_count(); ++i) {
    set_item(tab_at(i), tab_item());
    tab_at(i).invalidate_content();
  }
  reveal_selected_ = true;
  invalidate_content();
}

auto tab_bar::set_background(rgba tokens::color_set::*color) -> void {
  background_ = color;
  for (overflow_button *const control : controls_) {
    if (control != nullptr) {
      control->set_background(color);
    }
  }
  invalidate_paint();
}

auto tab_bar::tab_at(std::size_t index) const -> tab & { return static_cast<tab &>(child(index)); }

auto tab_bar::selected_index() const -> std::optional<std::size_t> {
  for (std::size_t i = 0; i < tab_count(); ++i) {
    if (tab_at(i).selected()) {
      return i;
    }
  }
  return std::nullopt;
}

auto tab_bar::select(std::size_t index) -> bool {
  if (index >= tab_count()) {
    return false;
  }
  auto const previous = selected_index();
  for (std::size_t i = 0; i < tab_count(); ++i) {
    tab_at(i).set_selected(i == index);
  }
  reveal_selected_ = true;
  invalidate_layout();
  if (tree() != nullptr) {
    // Tab re-enters the group at the selected tab.
    (void)tree()->focus().set_group_entry(*this, tab_at(index));
  }
  if (previous != index && on_select_) {
    auto const handler = on_select_;
    handler(index);
  }
  return true;
}

auto tab_bar::accessibility_info() const -> accessibility::info {
  accessibility::info details;
  details.kind = accessibility::role::tab_list;
  details.name = name_;
  details.orientation = accessibility::orientation::horizontal;
  return details;
}

auto tab_bar::view_width() const -> float { return bounds().width() - lead_ - trail_; }

auto tab_bar::max_scroll_offset() const -> float {
  return std::max(0.0f, content_width_ - view_width());
}

// Tabs sit at `lead_ - offset_` from where the row places them.
auto tab_bar::content_left(std::size_t index) const -> float {
  return tab_at(index).bounds().left() + offset_ - lead_;
}

auto tab_bar::content_right(std::size_t index) const -> float {
  return tab_at(index).bounds().right() + offset_ - lead_;
}

auto tab_bar::shift_tabs(float delta) -> void {
  for (std::size_t i = 0; i < tab_count(); ++i) {
    tab &t = tab_at(i);
    rect area = t.bounds();
    area.origin.x -= delta;
    t.set_bounds(area);
  }
}

auto tab_bar::set_scroll_offset(float offset) -> bool {
  float const next = std::clamp(snap_to_device(offset, scale()), 0.0f, max_scroll_offset());
  if (next == offset_) {
    return false;
  }
  float const delta = next - offset_;
  offset_ = next;
  if (!needs_layout()) {
    shift_tabs(delta);
    controls_[back_control]->set_enabled(offset_ > 0);
    controls_[forward_control]->set_enabled(offset_ < max_scroll_offset());
  }
  invalidate_paint();
  return true;
}

auto tab_bar::scroll_tab_into_view(std::size_t index) -> void {
  if (index >= tab_count()) {
    return;
  }
  if (needs_layout()) {
    reveal_selected_ = true;
    return;
  }
  set_scroll_offset(revealed(content_left(index), content_right(index), offset_, view_width(),
                             resolve(fixed(reveal_margin))));
}

auto tab_bar::scroll_toward_end() -> bool {
  if (needs_layout()) {
    return false;
  }
  float const edge = offset_ + view_width();
  for (std::size_t i = 0; i < tab_count(); ++i) {
    if (content_right(i) > edge + hidden_tolerance) {
      return set_scroll_offset(content_right(i) - view_width());
    }
  }
  return false;
}

auto tab_bar::scroll_toward_start() -> bool {
  if (needs_layout()) {
    return false;
  }
  for (std::size_t i = tab_count(); i-- > 0;) {
    if (content_left(i) < offset_ - hidden_tolerance) {
      return set_scroll_offset(content_left(i));
    }
  }
  return false;
}

auto tab_bar::tab_menu_open() const -> bool {
  overlay_host const *const host = overlay_host::find(*this);
  return host != nullptr && menu_popup_.valid() && host->is_open(menu_popup_);
}

auto tab_bar::open_tab_menu() -> bool {
  overlay_host *const host = overlay_host::find(*this);
  overflow_button *const button = controls_[menu_control];
  if (host == nullptr || button == nullptr || !button->visible() || tab_menu_open()) {
    return false;
  }
  auto list = std::make_unique<menu>("All tabs");
  auto const selected = selected_index();
  for (std::size_t i = 0; i < tab_count(); ++i) {
    list->add_item(std::to_string(i), tab_at(i).label(), {}, selected == i);
  }
  list->set_on_activate([this](std::string const &id) {
    close_tab_menu();
    auto const index = static_cast<std::size_t>(std::stoul(id));
    select(index);
    scroll_tab_into_view(index);
  });
  list->set_on_dismiss([this] { close_tab_menu(); });

  float const row = resolve(theme().sizes.dimension_height_element_min);
  auto shown = std::make_unique<option_list>(resolve(fixed(menu_min_width)), row * menu_rows);
  shown->set_list(std::move(list));
  rect const anchor = button->bounds_in_root();
  menu_popup_ =
      host->show_popup(std::move(shown), {host->from_root(anchor.origin), anchor.extent}, [this] {
        menu_popup_ = {};
        if (controls_[menu_control] != nullptr) {
          controls_[menu_control]->set_expanded(false);
        }
      });
  button->set_expanded(true);
  return true;
}

auto tab_bar::close_tab_menu() -> void {
  if (overlay_host *const host = overlay_host::find(*this);
      host != nullptr && menu_popup_.valid()) {
    host->dismiss(menu_popup_);
  }
}

auto tab_bar::place_controls() -> void {
  if (controls_[back_control] == nullptr) {
    return;
  }
  float const width = bounds().width();
  float const height = bounds().height();
  float const button = resolve(fixed(control_width));
  scale_factors const factors = scale();
  bool const has_menu = controls_[menu_control]->visible();
  controls_[back_control]->set_bounds(snap_to_device(rect{{0, 0}, {lead_, height}}, factors));
  controls_[forward_control]->set_bounds(
      snap_to_device(rect{{width - trail_, 0}, {button, height}}, factors));
  if (has_menu) {
    controls_[menu_control]->set_bounds(
        snap_to_device(rect{{width - button, 0}, {button, height}}, factors));
  }
  controls_[back_control]->set_enabled(offset_ > 0);
  controls_[forward_control]->set_enabled(offset_ < max_scroll_offset());
}

auto tab_bar::on_layout() -> void {
  box::on_layout();
  // The box has just placed the tabs unscrolled.
  float width = 0;
  for (std::size_t i = 0; i < tab_count(); ++i) {
    if (tab_at(i).visible()) {
      width = std::max(width, tab_at(i).bounds().right());
    }
  }
  content_width_ = width;

  overflowing_ = style_ == tab_bar_style::line && content_width_ > bounds().width();
  bool const has_menu = overflowing_ && overlay_host::find(*this) != nullptr;
  float const button = resolve(fixed(control_width));
  lead_ = overflowing_ ? button : 0;
  trail_ = overflowing_ ? (has_menu ? 2 * button : button) : 0;
  if (controls_[back_control] != nullptr) {
    controls_[back_control]->set_visible(overflowing_);
    controls_[forward_control]->set_visible(overflowing_);
    controls_[menu_control]->set_visible(has_menu);
  }

  float wanted = offset_;
  if (reveal_selected_) {
    reveal_selected_ = false;
    if (auto const index = selected_index()) {
      rect const area = tab_at(*index).bounds();
      wanted =
          revealed(area.left(), area.right(), wanted, view_width(), resolve(fixed(reveal_margin)));
    }
  }
  offset_ = std::clamp(snap_to_device(wanted, scale()), 0.0f, max_scroll_offset());
  if (offset_ != lead_) {
    shift_tabs(offset_ - lead_);
  }
  place_controls();
}

auto tab_bar::on_event(event_context &, event const &e) -> bool {
  if (auto const *const w = std::get_if<wheel_event>(&e)) {
    float delta = w->delta.x != 0 ? w->delta.x : w->delta.y;
    if (std::abs(w->delta.y) > std::abs(w->delta.x) && !has(w->modifiers, modifier::shift)) {
      delta = w->delta.y;
    }
    if (!w->precise) {
      delta *= resolve(fixed(wheel_step));
    }
    return set_scroll_offset(offset_ + delta);
  }
  auto const *const k = std::get_if<key_event>(&e);
  if (k == nullptr || !k->pressed || k->modifiers != modifier::none || tree() == nullptr) {
    return false;
  }
  focus_manager &focus = tree()->focus();
  switch (k->code) {
  case key::right:
    return focus.move_focus_in_group(focus_move::next);
  case key::left:
    return focus.move_focus_in_group(focus_move::previous);
  case key::home:
    return focus.move_focus_in_group(focus_move::first);
  case key::end:
    return focus.move_focus_in_group(focus_move::last);
  case key::down:
    return !k->repeat && open_tab_menu();
  default:
    return false;
  }
}

auto tab_bar::on_measure(constraints const &limits) const -> basalt::size {
  auto measured = box::on_measure(limits);
  if (style_ == tab_bar_style::line) {
    measured.height = std::max(measured.height, resolve(theme().sizes.dimension_height_tab));
  }
  return measured;
}

auto tab_bar::on_paint(render_context &context) const -> void {
  if (tree() == nullptr) {
    return;
  }
  basalt::theme const &t = theme();
  float const stroke = device_stroke_width(fixed(1), scale());
  if (style_ == tab_bar_style::segmented) {
    rect const area{{}, bounds().extent};
    context.stroke_rounded_rect(inset(area, insets::uniform(stroke / 2)),
                                resolve(t.sizes.radius_default), t.colors.border_subdued, stroke);
    return;
  }
  context.fill_rect({{0, bounds().height() - stroke}, {bounds().width(), stroke}},
                    t.colors.border_subdued);
}

auto tab_bar::on_paint_overlay(render_context &context) const -> void {
  if (!overflowing_ || tree() == nullptr) {
    return;
  }
  float const strip = resolve(fixed(fade_width)) / fade_strips;
  float const height = bounds().height() - device_stroke_width(fixed(1), scale());
  rgba const background = theme().colors.*background_;
  auto const fade = [&](float edge, float direction) {
    for (int i = 0; i < fade_strips; ++i) {
      float const opacity = static_cast<float>(fade_strips - i) / (fade_strips + 1);
      float const near = edge + direction * strip * static_cast<float>(i);
      float const left = direction > 0 ? near : near - strip;
      context.fill_rect({{left, 0}, {strip, height}}, with_opacity(background, opacity));
    }
  };
  if (offset_ > 0) {
    fade(lead_, 1);
  }
  if (offset_ < max_scroll_offset()) {
    fade(bounds().width() - trail_, -1);
  }
}

} // namespace basalt
