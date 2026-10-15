#pragma once

#include <cstdint>
#include <string_view>

namespace basalt::accessibility {

// What a widget is, for assistive technology. Platform bridges map each role
// to the native equivalent.
enum class role : std::uint8_t {
  // The widget has no semantics of its own. It is left out of the semantic
  // tree and its children join its parent.
  none,
  window,
  dialog,
  alert,
  group,
  toolbar,
  label,
  heading,
  paragraph,
  document,
  image,
  separator,
  button,
  link,
  check_box,
  radio_button,
  // An on/off control that takes effect immediately.
  switch_control,
  text_field,
  combo_box,
  slider,
  progress_bar,
  scroll_area,
  scroll_bar,
  splitter,
  status_bar,
  menu_bar,
  menu,
  menu_item,
  menu_item_check,
  menu_item_radio,
  tab_list,
  tab,
  tab_panel,
  list,
  list_item,
  list_box,
  tree,
  tree_item,
  tooltip,
  navigation,
};

[[nodiscard]] auto to_string(role r) -> std::string_view;

// Roles the user operates: they take keyboard focus and must have a name.
[[nodiscard]] constexpr auto is_interactive(role r) -> bool {
  switch (r) {
  case role::button:
  case role::link:
  case role::check_box:
  case role::radio_button:
  case role::switch_control:
  case role::text_field:
  case role::combo_box:
  case role::slider:
  case role::splitter:
  case role::menu_item:
  case role::menu_item_check:
  case role::menu_item_radio:
  case role::tab:
    return true;
  default:
    return false;
  }
}

// Roles that must carry an accessible name, which is announced when the
// widget is reached.
[[nodiscard]] constexpr auto requires_name(role r) -> bool {
  switch (r) {
  case role::dialog:
  case role::toolbar:
  case role::menu_bar:
  case role::menu:
  case role::tab_list:
  case role::list:
  case role::list_box:
  case role::tree:
  case role::navigation:
  case role::scroll_area:
  case role::progress_bar:
    return true;
  default:
    return is_interactive(r);
  }
}

} // namespace basalt::accessibility
