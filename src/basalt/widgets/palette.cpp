#include "basalt/widgets/palette.h"

#include "basalt/event_router.h"
#include "basalt/render_context.h"
#include "basalt/widget_tree.h"
#include "basalt/widgets/label.h"
#include "basalt/widgets/text_line.h"

#include <algorithm>
#include <utility>
#include <variant>

namespace basalt {

namespace {} // namespace

class palette_row : public widget {
public:
  palette_row(palette_entry entry, std::size_t index, bool selected)
      : label_(entry.label, text_role::body), category_(entry.category, text_role::small),
        detail_(entry.state.empty()    ? entry.detail
                : entry.detail.empty() ? entry.state
                                       : entry.state + "   " + entry.detail,
                text_role::small),
        entry_(std::move(entry)), index_(index), selected_(selected) {
    set_minimum_target(true);
  }

  [[nodiscard]] auto accessibility_info() const -> accessibility::info override {
    accessibility::info details;
    details.kind = accessibility::role::list_item;
    details.name = entry_.label;
    details.keyboard_shortcut = entry_.detail;
    details.selected = selected_;
    details.actions = {accessibility::action::press, accessibility::action::select};
    return details;
  }

protected:
  auto on_event(event_context &, event const &e) -> bool override {
    auto const *const p = std::get_if<pointer_event>(&e);
    if (p == nullptr || p->button != pointer_button::primary) {
      return false;
    }
    if (p->action == pointer_action::down) {
      return true;
    }
    if (p->action == pointer_action::up) {
      if (interaction().pressed) {
        choose();
      }
      return true;
    }
    return false;
  }

  auto on_accessibility_action(accessibility::action_request const &request) -> bool override {
    if (request.kind == accessibility::action::press ||
        request.kind == accessibility::action::select) {
      choose();
      return true;
    }
    return false;
  }

  [[nodiscard]] auto on_measure(constraints const &) const -> basalt::size override {
    auto const &sizes = theme().sizes;
    basalt::size const label = label_.measure(*this);
    basalt::size const detail = detail_.measure(*this);
    basalt::size const category = category_.measure(*this);
    float const width =
        2 * resolve(sizes.inset_horizontal_list_item) + label.width +
        (category.width > 0 ? resolve(sizes.gap_horizontal_text_text) + category.width : 0) +
        (detail.width > 0 ? resolve(sizes.gap_horizontal_text_text) + detail.width : 0);
    return {width, std::max(label.height + 2 * resolve(sizes.inset_vertical_list_item_text),
                            resolve(sizes.dimension_height_element_min))};
  }

  auto on_paint(render_context &context) const -> void override {
    if (tree() == nullptr) {
      return;
    }
    basalt::theme const &t = theme();
    rect const area{{}, bounds().extent};
    if (selected_) {
      context.fill_rect(area, t.colors.background_accent_subdued);
    } else if (interaction().hovered) {
      context.fill_rect(area, t.colors.background_accent_transparent_hover);
    }
    float const inset_x = resolve(t.sizes.inset_horizontal_list_item);
    rect const text_area{{inset_x, 0}, {area.width() - 2 * inset_x, area.height()}};
    label_.draw_in(context, *this, text_area,
                   entry_.dimmed ? t.colors.text_subdued : t.colors.text_default);
    if (!entry_.category.empty()) {
      float const shift = label_.measure(*this).width + resolve(t.sizes.gap_horizontal_text_text);
      category_.draw_in(context, *this,
                        {{text_area.origin.x + shift, text_area.origin.y},
                         {std::max(0.0f, text_area.width() - shift), text_area.height()}},
                        t.colors.text_subdued);
    }
    detail_.draw_in(context, *this, text_area, t.colors.text_subdued, alignment::end);
  }

private:
  auto choose() -> void {
    if (auto *const owner = find_ancestor<palette>()) {
      owner->row_chosen(index_);
    }
  }

  text_line label_;
  text_line category_;
  text_line detail_;
  palette_entry entry_;
  std::size_t index_;
  bool selected_;
};

palette::palette(std::string name) : box(axis::vertical), name_(std::move(name)) {
  auto const &sizes = tokens::medium_sizes;
  set_padding(length_insets::symmetric(sizes.inset_horizontal_dialog, fixed(8)));
  set_gap(fixed(8));
  set_focus_scope(focus_scope::group);

  field_ = &emplace_child<text_field>();
  field_->set_accessible_name(name_);
  field_->set_preferred_width(fixed(560));
  field_->set_on_change([this](std::string const &text) {
    if (on_query_change_) {
      auto const handler = on_query_change_;
      handler(text);
    }
  });
  field_->set_on_submit([this](std::string const &) { (void)choose_selected(); });

  rows_ = &emplace_child<box>(axis::vertical);
  rebuild_rows();
}

auto palette::set_entries(std::vector<palette_entry> entries) -> void {
  entries_ = std::move(entries);
  selected_ = 0;
  first_visible_ = 0;
  rebuild_rows();
}

auto palette::set_empty_message(std::string message) -> void {
  empty_message_ = std::move(message);
  if (entries_.empty()) {
    rebuild_rows();
  }
}

auto palette::select(std::size_t index) -> bool {
  if (index >= entries_.size()) {
    return false;
  }
  selected_ = index;
  if (selected_ < first_visible_) {
    first_visible_ = selected_;
  } else if (selected_ >= first_visible_ + visible_rows) {
    first_visible_ = selected_ + 1 - visible_rows;
  }
  rebuild_rows();
  return true;
}

auto palette::move_selection(int delta) -> void {
  if (entries_.empty()) {
    return;
  }
  // Wraps at both ends, as menus do.
  auto const count = static_cast<long long>(entries_.size());
  auto next = (static_cast<long long>(selected_) + delta) % count;
  if (next < 0) {
    next += count;
  }
  (void)select(static_cast<std::size_t>(next));
}

auto palette::choose_selected() -> bool {
  if (selected_ >= entries_.size()) {
    return false;
  }
  row_chosen(selected_);
  return true;
}

auto palette::row_chosen(std::size_t entry_index) -> void {
  if (entry_index >= entries_.size() || !on_choose_) {
    return;
  }
  // The handler may dismiss the palette, so work from copies.
  auto const handler = on_choose_;
  auto const id = entries_[entry_index].id;
  handler(id);
}

auto palette::rebuild_rows() -> void {
  while (rows_->child_count() > 0) {
    (void)rows_->remove_child(rows_->child(rows_->child_count() - 1));
  }
  if (entries_.empty()) {
    rows_->emplace_child<label>(empty_message_);
  } else {
    auto const last = std::min(entries_.size(), first_visible_ + visible_rows);
    for (std::size_t i = first_visible_; i < last; ++i) {
      rows_->emplace_child<palette_row>(entries_[i], i, i == selected_);
    }
  }
  invalidate_content();
}

auto palette::on_event(event_context &, event const &e) -> bool {
  auto const *const k = std::get_if<key_event>(&e);
  if (k == nullptr || !k->pressed || k->modifiers != modifier::none) {
    return false;
  }
  switch (k->code) {
  case key::up:
    move_selection(-1);
    return true;
  case key::down:
    move_selection(1);
    return true;
  case key::page_up:
    move_selection(-static_cast<int>(visible_rows));
    return true;
  case key::page_down:
    move_selection(static_cast<int>(visible_rows));
    return true;
  default:
    return false;
  }
}

auto palette::on_paint(render_context &context) const -> void {
  if (tree() == nullptr) {
    return;
  }
  basalt::theme const &t = theme();
  rect const area{{}, bounds().extent};
  float const radius = resolve(t.sizes.radius_panel);
  float const width = device_stroke_width(fixed(1), scale());
  context.fill_rounded_rect(area, radius, t.colors.background_dialog);
  context.stroke_rounded_rect(inset(area, insets::uniform(width / 2)), radius,
                              t.colors.border_default, width);
}

auto palette::accessibility_info() const -> accessibility::info {
  accessibility::info details;
  details.kind = accessibility::role::dialog;
  details.name = name_;
  return details;
}

} // namespace basalt
