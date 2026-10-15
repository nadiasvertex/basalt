#include "basalt/widgets/diagnostics_list.h"

#include "basalt/event_router.h"
#include "basalt/render_context.h"
#include "basalt/widget_tree.h"
#include "basalt/widgets/scroll_view.h"

#include <algorithm>
#include <utility>
#include <variant>

namespace basalt {

namespace {

auto theme_of(widget const &w) -> theme const & {
  static theme const fallback;
  return w.tree() != nullptr ? w.tree()->theme() : fallback;
}

auto severity_name(diagnostic_severity severity) -> char const * {
  switch (severity) {
  case diagnostic_severity::error:
    return "Error";
  case diagnostic_severity::warning:
    return "Warning";
  case diagnostic_severity::info:
    break;
  }
  return "Note";
}

} // namespace

class diagnostic_row : public widget {
public:
  explicit diagnostic_row(diagnostic_entry entry)
      : entry_(std::move(entry)), message_(entry_.message, text_role::body),
        location_(entry_.location, text_role::small) {
    set_focus_policy(focus_policy::strong);
    set_minimum_target(true);
  }

  [[nodiscard]] auto accessibility_info() const -> accessibility::info override {
    accessibility::info details;
    details.kind = accessibility::role::list_item;
    details.name = std::string(severity_name(entry_.severity)) + ": " + entry_.message;
    details.description = entry_.location;
    details.selected = has_focus();
    details.actions = {accessibility::action::press, accessibility::action::select,
                       accessibility::action::focus};
    if (parent() != nullptr) {
      if (auto const index = parent()->index_of(*this)) {
        details.position_in_set = static_cast<std::uint32_t>(*index + 1);
        details.set_size = static_cast<std::uint32_t>(parent()->child_count());
      }
    }
    return details;
  }

protected:
  auto on_event(event_context &, event const &e) -> bool override {
    if (auto const *p = std::get_if<pointer_event>(&e)) {
      if (p->button != pointer_button::primary) {
        return false;
      }
      if (p->action == pointer_action::down) {
        return true;
      }
      if (p->action == pointer_action::up) {
        if (interaction().pressed) {
          activate();
        }
        return true;
      }
      return false;
    }
    if (auto const *k = std::get_if<key_event>(&e)) {
      if (k->pressed && !k->repeat && k->modifiers == modifier::none &&
          (k->code == key::enter || k->code == key::space)) {
        activate();
        return true;
      }
    }
    return false;
  }

  auto on_accessibility_action(accessibility::action_request const &request) -> bool override {
    if (request.kind == accessibility::action::press ||
        request.kind == accessibility::action::select) {
      activate();
      return true;
    }
    return false;
  }

  auto on_interaction_changed(interaction_state const &previous) -> void override {
    widget::on_interaction_changed(previous);
    if (interaction().focused && !previous.focused) {
      scroll_into_view();
    }
  }

  [[nodiscard]] auto on_measure(constraints const &) const -> basalt::size override {
    auto const &sizes = theme_of(*this).sizes;
    basalt::size const message = message_.measure(*this);
    basalt::size const location = location_.measure(*this);
    float const gap = resolve(sizes.gap_horizontal_text_text);
    float const width = 2 * resolve(sizes.inset_horizontal_list_item) + icon_extent() +
                        resolve(sizes.gap_horizontal_shape_text) + message.width +
                        (location.width > 0 ? gap + location.width : 0);
    return {width, std::max(message.height + 2 * resolve(sizes.inset_vertical_list_item_text),
                            resolve(sizes.dimension_height_element_min))};
  }

  auto on_paint(render_context &context) const -> void override {
    if (tree() == nullptr) {
      return;
    }
    theme const &t = theme_of(*this);
    rect const area{{}, bounds().extent};
    if (has_focus()) {
      context.fill_rect(area, t.colors.background_accent_subdued);
    } else if (interaction().hovered) {
      context.fill_rect(area, t.colors.background_accent_transparent_hover);
    }
    float const inset_x = resolve(t.sizes.inset_horizontal_list_item);
    float const icon = icon_extent();
    draw_icon(context, {{inset_x, (area.height() - icon) / 2}, {icon, icon}}, t);
    float const text_left = inset_x + icon + resolve(t.sizes.gap_horizontal_shape_text);
    rect const text_area{{text_left, 0}, {area.width() - text_left - inset_x, area.height()}};
    message_.draw_in(context, *this, text_area, t.colors.text_default);
    location_.draw_in(context, *this, text_area, t.colors.text_subdued, alignment::end);
  }

private:
  [[nodiscard]] auto icon_extent() const -> float {
    return resolve(theme_of(*this).sizes.dimension_icon_small);
  }

  // Errors are filled circles, warnings triangles and notes ringed circles.
  auto draw_icon(render_context &context, rect box, theme const &t) const -> void {
    float const stroke = device_stroke_width(fixed(2), scale());
    switch (entry_.severity) {
    case diagnostic_severity::error:
      context.fill_rounded_rect(box, box.width() / 2, t.colors.icon_danger);
      break;
    case diagnostic_severity::warning: {
      point const top{box.center().x, box.top()};
      point const left{box.left(), box.bottom()};
      point const right{box.right(), box.bottom()};
      context.line(top, left, t.colors.icon_warning, stroke);
      context.line(left, right, t.colors.icon_warning, stroke);
      context.line(right, top, t.colors.icon_warning, stroke);
      break;
    }
    case diagnostic_severity::info:
      context.stroke_rounded_rect(inset(box, insets::uniform(stroke / 2)), box.width() / 2,
                                  t.colors.icon_interactive, stroke);
      break;
    }
  }

  auto activate() -> void {
    if (auto *const list = dynamic_cast<diagnostics_list *>(parent())) {
      if (auto const index = list->index_of(*this)) {
        list->row_activated(*index);
      }
    }
  }

  auto scroll_into_view() -> void;

  diagnostic_entry entry_;
  text_line message_;
  text_line location_;
};

diagnostics_list::diagnostics_list(std::string name)
    : box(axis::vertical), name_(std::move(name)), empty_("No problems", text_role::body) {
  set_focus_scope(focus_scope::group);
}

auto diagnostics_list::set_entries(std::vector<diagnostic_entry> entries) -> void {
  bool const had_focus = interaction().focus_within;
  entries_ = std::move(entries);
  while (child_count() > 0) {
    (void)remove_child(child(child_count() - 1));
  }
  for (auto const &entry : entries_) {
    emplace_child<diagnostic_row>(entry);
  }
  if (had_focus && child_count() > 0) {
    child(0).request_focus();
  }
  invalidate_paint();
}

auto diagnostics_list::focused_index() const -> std::optional<std::size_t> {
  for (std::size_t i = 0; i < child_count(); ++i) {
    if (child(i).has_focus()) {
      return i;
    }
  }
  return std::nullopt;
}

auto diagnostics_list::focus_row(std::size_t index) -> bool {
  return index < child_count() && child(index).request_focus();
}

auto diagnostics_list::set_empty_message(std::string message) -> void {
  if (empty_.set_text(std::move(message))) {
    invalidate_layout();
    invalidate_paint();
  }
}

auto diagnostics_list::row_activated(std::size_t index) -> void {
  if (index < entries_.size() && on_activate_) {
    // The handler may replace the entries, so work on copies.
    auto const handler = on_activate_;
    auto const entry = entries_[index];
    handler(index, entry);
  }
}

auto diagnostics_list::accessibility_info() const -> accessibility::info {
  accessibility::info details;
  details.kind = accessibility::role::list;
  details.name = name_;
  details.orientation = accessibility::orientation::vertical;
  return details;
}

auto diagnostics_list::on_event(event_context &, event const &e) -> bool {
  auto const *const k = std::get_if<key_event>(&e);
  if (k == nullptr || !k->pressed || k->modifiers != modifier::none || tree() == nullptr) {
    return false;
  }
  focus_manager &focus = tree()->focus();
  switch (k->code) {
  case key::down:
    return focus.move_focus_in_group(focus_move::next, false);
  case key::up:
    return focus.move_focus_in_group(focus_move::previous, false);
  case key::home:
    return focus.move_focus_in_group(focus_move::first);
  case key::end:
    return focus.move_focus_in_group(focus_move::last);
  default:
    return false;
  }
}

auto diagnostics_list::on_measure(constraints const &limits) const -> basalt::size {
  if (child_count() > 0) {
    return box::on_measure(limits);
  }
  basalt::size const text = empty_.measure(*this);
  auto const &sizes = theme_of(*this).sizes;
  return {text.width + 2 * resolve(sizes.inset_horizontal_list_item),
          std::max(text.height, resolve(sizes.dimension_height_element_min))};
}

auto diagnostics_list::on_paint(render_context &context) const -> void {
  if (tree() == nullptr || child_count() > 0) {
    return;
  }
  theme const &t = theme_of(*this);
  rect const area{{}, bounds().extent};
  empty_.draw_in(context, *this,
                 inset(area, insets::symmetric(resolve(t.sizes.inset_horizontal_list_item), 0)),
                 t.colors.text_subdued);
}

auto diagnostic_row::scroll_into_view() -> void {
  for (widget *w = parent(); w != nullptr; w = w->parent()) {
    if (auto *const view = dynamic_cast<scroll_view *>(w)) {
      if (widget *const content = view->content()) {
        view->scroll_into_view(
            translated(rect{{}, bounds().extent}, to_root({}) - content->to_root({})));
      }
      return;
    }
  }
}

} // namespace basalt
