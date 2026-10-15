#include "basalt/widgets/navigation_bar.h"

#include "basalt/event_router.h"
#include "basalt/widget_tree.h"
#include "basalt/widgets/menu.h"
#include "basalt/widgets/overlay_host.h"

#include <algorithm>
#include <variant>

#include "glyphs.h"
#include "notify.h"

namespace basalt {

namespace a11y = accessibility;

navigation_item::navigation_item(std::string id, std::string label)
    : id_(std::move(id)), label_(std::move(label), text_role::body) {
  set_focus_policy(focus_policy::strong);
  set_minimum_target(true);
}

navigation_item::~navigation_item() = default;

auto navigation_item::set_label(std::string label) -> void {
  if (label_.set_text(std::move(label))) {
    invalidate_content();
  }
}

auto navigation_item::set_selected(bool selected) -> void {
  if (selected != selected_) {
    selected_ = selected;
    label_.set_role(selected ? text_role::body_strong : text_role::body);
    invalidate_content();
  }
}

auto navigation_item::add_entry(std::string id, std::string label) -> void {
  entries_.push_back({std::move(id), std::move(label)});
  invalidate_paint();
}

auto navigation_item::is_open() const -> bool {
  overlay_host const *const host = overlay_host::find(*this);
  return host != nullptr && popup_.valid() && host->is_open(popup_);
}

auto navigation_item::close() -> void {
  if (overlay_host *const host = overlay_host::find(*this); host != nullptr && popup_.valid()) {
    host->dismiss(std::exchange(popup_, {}));
  }
  popup_ = {};
}

auto navigation_item::on_detached() -> void { close(); }

auto navigation_item::open_entries() -> bool {
  overlay_host *const host = overlay_host::find(*this);
  if (host == nullptr) {
    return false;
  }
  auto popup = std::make_unique<menu>(label_.text());
  for (entry const &e : entries_) {
    popup->add_item(e.id, e.label);
  }
  popup->set_on_activate([this](std::string const &id) {
    close();
    if (auto *const bar = find_ancestor<navigation_bar>()) {
      bar->chosen(*this, id);
    }
  });
  popup->set_on_dismiss([this] { close(); });
  rect const anchor = bounds_in_root();
  rect const placed{host->from_root(anchor.origin), anchor.extent};
  popup_ = host->show_popup(std::move(popup), placed, [this] { popup_ = {}; });
  return true;
}

auto navigation_item::activate() -> bool {
  if (!enabled_in_tree()) {
    return false;
  }
  if (!entries_.empty()) {
    if (is_open()) {
      close();
      return true;
    }
    return open_entries();
  }
  if (auto *const bar = find_ancestor<navigation_bar>()) {
    bar->chosen(*this, id_);
  }
  return true;
}

auto navigation_item::accessibility_info() const -> a11y::info {
  a11y::info details;
  details.kind = a11y::role::button;
  details.name = label_.text();
  details.selected = selected_;
  if (!entries_.empty()) {
    details.expanded = is_open();
  }
  details.actions = {a11y::action::press, a11y::action::focus};
  return details;
}

auto navigation_item::on_accessibility_action(a11y::action_request const &request) -> bool {
  return request.kind == a11y::action::press && activate();
}

auto navigation_item::on_event(event_context &, event const &e) -> bool {
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
        (k->code == key::enter || k->code == key::space ||
         (k->code == key::down && !entries_.empty()))) {
      activate();
      return true;
    }
  }
  return false;
}

auto navigation_item::on_measure(constraints const &) const -> basalt::size {
  basalt::theme const &t = theme();
  basalt::size const text = label_.measure(*this);
  float const mark = entries_.empty() ? 0 : resolve(t.sizes.dimension_icon);
  return {2 * resolve(t.sizes.inset_horizontal_nav_bar_item) + text.width + mark,
          std::max(text.height, resolve(t.sizes.dimension_height_element_min))};
}

auto navigation_item::on_paint(render_context &context) const -> void {
  if (tree() == nullptr) {
    return;
  }
  basalt::theme const &t = theme();
  rect const area{{}, bounds().extent};
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
  float const inset = resolve(t.sizes.inset_horizontal_nav_bar_item);
  float const mark = entries_.empty() ? 0 : resolve(t.sizes.dimension_icon);
  rect const text_area{{inset, 0},
                       {std::max(0.0f, area.width() - 2 * inset - mark), area.height()}};
  label_.draw_in(context, *this, text_area,
                 selected_ ? t.colors.text_interactive : t.colors.text_default);
  if (mark > 0) {
    glyphs::draw_chevron(
        context, {{area.width() - inset - mark, (area.height() - mark) / 2}, {mark, mark}},
        !is_open(), t.colors.icon_default, device_stroke_width(fixed(1.5f), scale()));
  }
  if (selected_) {
    float const bar = device_stroke_width(fixed(2), scale());
    context.fill_rect({{0, area.height() - bar}, {area.width(), bar}}, t.colors.border_accent);
  }
}

navigation_bar::navigation_bar(std::string name) : box(axis::horizontal), name_(std::move(name)) {
  set_padding(length_insets::symmetric(fixed(8), fixed(4)));
  set_gap(fixed(8));
  set_cross_alignment(alignment::center);
  leading_ = &emplace_child<box>(axis::horizontal);
  leading_->set_gap(fixed(4));
  title_ = &emplace_child<label>(std::string{}, text_role::heading_small);
  sections_ = &emplace_child<box>(axis::horizontal);
  center_ = &add(std::make_unique<box>(axis::horizontal), box_item{.grow = 1});
  center_->set_main_alignment(alignment::center);
  center_->set_gap(fixed(4));
  utility_ = &emplace_child<box>(axis::horizontal);
  title_->set_visible(false);
}

auto navigation_bar::set_layout(navigation_layout layout) -> void {
  layout_ = layout;
  bool const sectioned = layout == navigation_layout::sections;
  sections_->set_visible(sectioned);
  utility_->set_visible(sectioned);
}

auto navigation_bar::set_title(std::string title) -> void {
  title_->set_text(std::move(title));
  title_->set_visible(!title_->text().empty());
}

auto navigation_bar::add_section(std::string id, std::string label) -> navigation_item & {
  auto &added = sections_->emplace_child<navigation_item>(std::move(id), std::move(label));
  added.set_selected(selected_ && *selected_ == added.item_id());
  return added;
}

auto navigation_bar::add_utility_link(std::string id, std::string label) -> navigation_item & {
  return utility_->emplace_child<navigation_item>(std::move(id), std::move(label));
}

auto navigation_bar::find_item(std::string const &id) const -> navigation_item * {
  for (box *const group : {sections_, utility_}) {
    for (widget &c : group->children()) {
      if (auto *const item = dynamic_cast<navigation_item *>(&c);
          item != nullptr && item->item_id() == id) {
        return item;
      }
    }
  }
  return nullptr;
}

auto navigation_bar::set_avatar(std::unique_ptr<avatar> person) -> avatar & {
  if (avatar_ != nullptr) {
    (void)remove_child(*avatar_);
  }
  avatar_ = &add_child(std::move(person));
  return *avatar_;
}

auto navigation_bar::select(std::optional<std::string> id) -> void {
  selected_ = std::move(id);
  for (widget &c : sections_->children()) {
    if (auto *const item = dynamic_cast<navigation_item *>(&c)) {
      item->set_selected(false);
    }
  }
  if (selected_) {
    if (navigation_item *const item = find_item(*selected_);
        item != nullptr && item->parent() == sections_) {
      item->set_selected(true);
    }
  }
}

auto navigation_bar::chosen(navigation_item const &from, std::string const &id) -> void {
  if (from.parent() == sections_) {
    select(from.item_id());
  }
  auto const target = id;
  notify(on_navigate_, target);
}

auto navigation_bar::accessibility_info() const -> a11y::info {
  a11y::info details;
  details.kind = a11y::role::navigation;
  details.name = name_;
  details.orientation = a11y::orientation::horizontal;
  return details;
}

} // namespace basalt
