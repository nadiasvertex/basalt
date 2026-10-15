#include "basalt/widgets/select.h"

#include "basalt/event_router.h"
#include "basalt/widget_tree.h"
#include "basalt/widgets/menu.h"
#include "basalt/widgets/overlay_host.h"
#include "basalt/widgets/scroll_view.h"

#include <algorithm>
#include <numeric>
#include <utility>
#include <variant>

#include "glyphs.h"
#include "notify.h"

namespace basalt {

namespace a11y = accessibility;

namespace {

constexpr text_role helper_role = text_role::small;
// The list scrolls beyond this many rows.
constexpr std::size_t visible_rows = 8;

// The list shown in the popup: a menu in a scroll view that is as wide as the
// field and as tall as its rows, up to a limit.
class option_list : public widget {
public:
  option_list(float min_width, float max_height) : min_width_(min_width), max_height_(max_height) {
    set_hit_testable(false);
  }

  auto set_list(std::unique_ptr<menu> list) -> menu & {
    menu &added = *list;
    scroller_ = &emplace_child<scroll_view>(scroll_axes::vertical);
    scroller_->set_content(std::move(list));
    scroller_->set_scrollbars(true);
    return added;
  }

  [[nodiscard]] auto accessibility_info() const -> a11y::info override {
    a11y::info details;
    details.kind = a11y::role::list_box;
    return details;
  }

protected:
  [[nodiscard]] auto on_measure(constraints const &limits) const -> basalt::size override {
    basalt::size natural;
    if (scroller_ != nullptr && scroller_->content() != nullptr) {
      natural = scroller_->content()->measure(constraints::loose({limits.max.width, unbounded}));
    }
    return {std::max(natural.width, min_width_), std::min(natural.height, max_height_)};
  }

  auto on_layout() -> void override {
    if (scroller_ != nullptr) {
      scroller_->set_bounds({{}, bounds().extent});
    }
  }

private:
  float min_width_;
  float max_height_;
  scroll_view *scroller_ = nullptr;
};

} // namespace

struct select::layout_parts {
  basalt::size label;
  basalt::size helper;
  basalt::size value;
  float field_height = 0;
  float field_width = 0;
  float inset = 0;
  float mark = 0;
  float label_gap = 0;
  float helper_gap = 0;
  float field_top = 0;
};

select::select(std::string label, control_size size)
    : label_(std::move(label), label_role_for(size)), helper_({}, helper_role),
      placeholder_({}, label_role_for(size)), size_(size) {
  set_focus_policy(focus_policy::strong);
}

select::~select() = default;

auto select::parts() const -> layout_parts {
  basalt::theme const &t = theme();
  layout_parts p;
  p.label = label_.text().empty() ? basalt::size{} : label_.measure(*this);
  p.helper = helper_.text().empty() ? basalt::size{} : helper_.measure(*this);
  text_line shown{value_text(), label_role_for(size_)};
  p.value = shown.measure(*this);
  p.inset = resolve(t.sizes.inset_horizontal_input);
  p.mark = resolve(t.sizes.dimension_icon);
  p.label_gap = p.label.height > 0 ? resolve(t.sizes.gap_vertical_field_input) : 0;
  p.helper_gap = p.helper.height > 0 ? resolve(t.sizes.gap_vertical_field_helper_text) : 0;
  p.field_top = p.label.height + p.label_gap;
  p.field_height = std::max(p.value.height + 2 * resolve(t.sizes.inset_vertical_list_item_text),
                            resolve(t.sizes.dimension_height_element_min));
  float const clear = mode_ == select_mode::multiple && !chosen_indices().empty() ? p.mark : 0;
  p.field_width = std::max(2 * p.inset + p.value.width + clear + p.mark,
                           resolve(t.sizes.dimension_width_popover_min));
  return p;
}

auto select::value_text() const -> std::string {
  auto const chosen = chosen_indices();
  if (chosen.empty()) {
    return placeholder_.text();
  }
  if (mode_ == select_mode::multiple) {
    return std::to_string(chosen.size()) + " selected";
  }
  return options_[chosen.front()];
}

auto select::set_label(std::string label) -> void {
  if (label_.set_text(std::move(label))) {
    invalidate_content();
  }
}

auto select::set_helper_text(std::string text) -> void {
  if (helper_.set_text(std::move(text))) {
    invalidate_content();
  }
}

auto select::set_placeholder(std::string text) -> void {
  if (placeholder_.set_text(std::move(text))) {
    invalidate_content();
  }
}

auto select::set_error(bool error) -> void {
  if (error != error_) {
    error_ = error;
    invalidate_paint();
  }
}

auto select::set_size(control_size size) -> void {
  if (size == size_) {
    return;
  }
  size_ = size;
  label_.set_role(label_role_for(size));
  placeholder_.set_role(label_role_for(size));
  invalidate_content();
}

auto select::set_mode(select_mode mode) -> void {
  if (mode == mode_) {
    return;
  }
  mode_ = mode;
  if (mode_ == select_mode::single) {
    bool seen = false;
    for (std::size_t i = 0; i < chosen_.size(); ++i) {
      if (chosen_[i] && seen) {
        chosen_[i] = false;
      }
      seen = seen || chosen_[i];
    }
  }
  invalidate_content();
}

auto select::add_option(std::string label) -> void {
  options_.push_back(std::move(label));
  chosen_.push_back(false);
}

auto select::option_label(std::size_t index) const -> std::string const & {
  return options_[index];
}

auto select::is_chosen(std::size_t index) const -> bool {
  return index < chosen_.size() && chosen_[index];
}

auto select::chosen_indices() const -> std::vector<std::size_t> {
  std::vector<std::size_t> result;
  for (std::size_t i = 0; i < chosen_.size(); ++i) {
    if (chosen_[i]) {
      result.push_back(i);
    }
  }
  return result;
}

auto select::choose(std::size_t index, bool chosen) -> bool {
  if (index >= options_.size()) {
    return false;
  }
  if (chosen && mode_ == select_mode::single) {
    std::ranges::fill(chosen_, false);
  }
  chosen_[index] = chosen;
  invalidate_content();
  return true;
}

auto select::clear() -> void {
  std::ranges::fill(chosen_, false);
  invalidate_content();
}

auto select::changed() -> void { notify(on_change_); }

auto select::is_open() const -> bool {
  overlay_host const *const host = overlay_host::find(*this);
  return host != nullptr && popup_.valid() && host->is_open(popup_);
}

auto select::open() -> bool {
  if (!enabled_in_tree() || options_.empty() || is_open()) {
    return false;
  }
  overlay_host *const host = overlay_host::find(*this);
  if (host == nullptr) {
    return false;
  }
  basalt::theme const &t = theme();
  rect const field_box = {{0, parts().field_top}, {bounds().width(), parts().field_height}};
  float const row = resolve(t.sizes.dimension_height_element_min);

  // Chosen options come first when opening in the multiple mode.
  std::vector<std::size_t> order(options_.size());
  std::iota(order.begin(), order.end(), std::size_t{0});
  if (mode_ == select_mode::multiple) {
    std::ranges::stable_partition(order, [this](std::size_t i) { return chosen_[i]; });
  }

  auto list = std::make_unique<menu>(label_.text().empty() ? placeholder_.text() : label_.text());
  for (std::size_t index : order) {
    list->add_item(std::to_string(index), options_[index], {}, chosen_[index]);
  }
  list_ = list.get();
  list->set_on_activate([this](std::string const &id) { option_activated(std::stoul(id)); });
  list->set_on_dismiss([this] { close(); });

  auto shown = std::make_unique<option_list>(field_box.width(), row * visible_rows);
  shown->set_list(std::move(list));

  rect const anchor = bounds_in_root();
  rect const placed = {host->from_root({anchor.origin.x, anchor.origin.y + field_box.top()}),
                       field_box.extent};
  popup_ = host->show_popup(std::move(shown), placed, [this] {
    popup_ = {};
    list_ = nullptr;
  });
  invalidate_paint();
  return true;
}

auto select::close() -> void {
  if (overlay_host *const host = overlay_host::find(*this); host != nullptr && popup_.valid()) {
    host->dismiss(popup_);
  }
  popup_ = {};
  list_ = nullptr;
  if (tree() != nullptr) {
    (void)request_focus();
  }
  invalidate_paint();
}

auto select::option_activated(std::size_t index) -> void {
  if (index >= options_.size()) {
    return;
  }
  if (mode_ == select_mode::single) {
    choose(index);
    changed();
    close();
    return;
  }
  chosen_[index] = !chosen_[index];
  if (list_ != nullptr) {
    if (menu_item *const shown = list_->find_item(std::to_string(index))) {
      shown->set_checked(chosen_[index]);
    }
  }
  invalidate_content();
  changed();
}

auto select::on_detached() -> void {
  if (overlay_host *const host = overlay_host::find(*this); host != nullptr && popup_.valid()) {
    host->dismiss(popup_);
  }
  popup_ = {};
  list_ = nullptr;
}

auto select::accessibility_info() const -> a11y::info {
  a11y::info details;
  details.kind = a11y::role::combo_box;
  details.name = label_.text();
  details.description = helper_.text();
  details.value = value_text();
  details.invalid = error_;
  details.expanded = is_open();
  details.actions = {a11y::action::press, a11y::action::focus};
  return details;
}

auto select::on_accessibility_action(a11y::action_request const &request) -> bool {
  if (request.kind != a11y::action::press) {
    return false;
  }
  if (is_open()) {
    close();
    return true;
  }
  return open();
}

auto select::on_event(event_context &, event const &e) -> bool {
  if (auto const *p = std::get_if<pointer_event>(&e)) {
    if (p->button != pointer_button::primary) {
      return false;
    }
    if (p->action == pointer_action::down) {
      return true;
    }
    if (p->action == pointer_action::up) {
      if (interaction().pressed) {
        layout_parts const lp = parts();
        bool const has_clear = mode_ == select_mode::multiple && !chosen_indices().empty();
        float const clear_left = bounds().width() - lp.inset - 2 * lp.mark;
        if (has_clear && p->position.x >= clear_left && p->position.x < clear_left + lp.mark) {
          clear();
          changed();
        } else if (!open()) {
          close();
        }
      }
      return true;
    }
    return false;
  }
  if (auto const *k = std::get_if<key_event>(&e)) {
    if (!k->pressed || k->modifiers != modifier::none) {
      return false;
    }
    if (k->code == key::enter || k->code == key::space || k->code == key::down) {
      return !k->repeat && open();
    }
    if (mode_ == select_mode::multiple &&
        (k->code == key::delete_forward || k->code == key::backspace) &&
        !chosen_indices().empty()) {
      clear();
      changed();
      return true;
    }
  }
  return false;
}

auto select::on_measure(constraints const &) const -> basalt::size {
  layout_parts const p = parts();
  float const width = std::max({p.field_width, p.label.width, p.helper.width});
  return {width, p.field_top + p.field_height + p.helper_gap + p.helper.height};
}

auto select::on_paint(render_context &context) const -> void {
  if (tree() == nullptr) {
    return;
  }
  basalt::theme const &t = theme();
  layout_parts const p = parts();
  float const width = bounds().width();
  if (p.label.height > 0) {
    label_.draw(context, *this, {0, 0}, t.colors.text_default);
  }

  rect const field{{0, p.field_top}, {width, p.field_height}};
  float const radius = resolve(t.sizes.radius_default);
  float const stroke = device_stroke_width(fixed(1), scale());
  control_state const state = control_state_of(interaction(), enabled());
  context.fill_rounded_rect(field, radius,
                            state == control_state::hovered
                                ? t.colors.background_accent_transparent_hover
                                : t.colors.background_default);
  rgba const border = error_                       ? t.colors.border_danger
                      : (has_focus() || is_open()) ? t.colors.border_accent
                                                   : t.colors.border_default;
  context.stroke_rounded_rect(inset(field, insets::uniform(stroke / 2)), radius, border, stroke);

  bool const has_clear = mode_ == select_mode::multiple && !chosen_indices().empty();
  float const right = width - p.inset;
  float const chevron_left = right - p.mark;
  float const clear_left = chevron_left - p.mark;
  float const text_right = (has_clear ? clear_left : chevron_left) - p.inset;
  bool const showing_value = !chosen_indices().empty();
  rect const text_area{{p.inset, field.top()},
                       {std::max(0.0f, text_right - p.inset), field.height()}};
  text_line shown{value_text(), label_role_for(size_)};
  auto const clip = context.clip(text_area);
  shown.draw_in(context, *this, text_area,
                showing_value ? t.colors.text_default : t.colors.text_subdued);

  float const mark_width = device_stroke_width(fixed(1.5f), scale());
  float const mark_top = field.top() + (field.height() - p.mark) / 2;
  if (has_clear) {
    glyphs::draw_close(context, {{clear_left, mark_top}, {p.mark, p.mark}}, t.colors.icon_default,
                       mark_width);
  }
  glyphs::draw_chevron(context, {{chevron_left, mark_top}, {p.mark, p.mark}}, !is_open(),
                       t.colors.icon_default, mark_width);

  if (p.helper.height > 0) {
    helper_.draw(context, *this, {0, field.bottom() + p.helper_gap},
                 error_ ? t.colors.text_danger : t.colors.text_subdued);
  }
}

} // namespace basalt
