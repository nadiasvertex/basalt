#include "basalt/widgets/menu_bar.h"

#include "basalt/widget_tree.h"
#include "basalt/widgets/button.h"
#include "basalt/widgets/overlay_host.h"

#include <utility>
#include <variant>

namespace basalt {

menu_bar::menu_bar(std::string name) : box(axis::horizontal), name_(std::move(name)) {
  set_focus_scope(focus_scope::group);
  set_padding(length_insets::symmetric(fixed(8), fixed(4)));
  set_gap(fixed(4));
  set_cross_alignment(alignment::center);
}

auto menu_bar::add_menu(std::string title, populate_function populate) -> void {
  std::size_t const index = menus_.size();
  menus_.push_back({title, std::move(populate)});
  auto &titled = emplace_child<button>(std::move(title));
  titled.set_variant(button_variant::tertiary);
  titled.set_on_press([this, index] { open_menu(index); });
}

auto menu_bar::set_menu_visible(std::size_t index, bool visible) -> void {
  if (index >= menus_.size() || index >= child_count()) {
    return;
  }
  if (!visible) {
    close_menu();
  }
  child(index).set_visible(visible);
}

auto menu_bar::is_open() const -> bool {
  overlay_host const *const host = overlay_host::find(*this);
  return host != nullptr && open_popup_.valid() && host->is_open(open_popup_);
}

auto menu_bar::close_menu() -> void {
  if (overlay_host *const host = overlay_host::find(*this);
      host != nullptr && open_popup_.valid()) {
    host->dismiss(std::exchange(open_popup_, {}));
  }
}

auto menu_bar::open_menu(std::size_t index) -> bool {
  overlay_host *const host = overlay_host::find(*this);
  if (host == nullptr || index >= menus_.size() || index >= child_count()) {
    return false;
  }
  close_menu();

  auto popup = std::make_unique<menu>(menus_[index].title);
  menus_[index].populate(*popup);
  popup->set_on_activate([this](std::string const &id) {
    close_menu();
    if (on_command_) {
      auto const handler = on_command_;
      handler(id);
    }
  });
  popup->set_on_dismiss([this] { close_menu(); });

  widget const &title = child(index);
  rect const anchor = title.bounds_in_root();
  rect const placed = {host->from_root(anchor.origin), anchor.extent};
  open_popup_ = host->show_popup(std::move(popup), placed, [this] { open_popup_ = {}; });
  return true;
}

auto menu_bar::accessibility_info() const -> accessibility::info {
  accessibility::info details;
  details.kind = accessibility::role::menu_bar;
  details.name = name_;
  details.orientation = accessibility::orientation::horizontal;
  return details;
}

auto menu_bar::on_event(event_context &, event const &e) -> bool {
  auto const *const k = std::get_if<key_event>(&e);
  if (k == nullptr || !k->pressed || tree() == nullptr || k->modifiers != modifier::none) {
    return false;
  }
  switch (k->code) {
  case key::left:
    return tree()->focus().move_focus_in_group(focus_move::previous);
  case key::right:
    return tree()->focus().move_focus_in_group(focus_move::next);
  case key::home:
    return tree()->focus().move_focus_in_group(focus_move::first);
  case key::end:
    return tree()->focus().move_focus_in_group(focus_move::last);
  default:
    return false;
  }
}

auto menu_bar::on_paint(render_context &context) const -> void {
  if (tree() == nullptr) {
    return;
  }
  auto const &colors = tree()->theme().colors;
  rect const area{{}, bounds().extent};
  context.fill_rect(area, colors.background_secondary);
  float const width = device_stroke_width(fixed(1), scale());
  context.fill_rect({{0, area.height() - width}, {area.width(), width}}, colors.border_subdued);
}

} // namespace basalt
