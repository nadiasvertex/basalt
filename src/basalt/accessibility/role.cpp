#include "basalt/accessibility/role.h"

namespace basalt::accessibility {

auto to_string(role r) -> std::string_view {
  switch (r) {
  case role::none:
    return "none";
  case role::window:
    return "window";
  case role::dialog:
    return "dialog";
  case role::alert:
    return "alert";
  case role::group:
    return "group";
  case role::toolbar:
    return "toolbar";
  case role::label:
    return "label";
  case role::heading:
    return "heading";
  case role::paragraph:
    return "paragraph";
  case role::document:
    return "document";
  case role::image:
    return "image";
  case role::separator:
    return "separator";
  case role::button:
    return "button";
  case role::link:
    return "link";
  case role::check_box:
    return "check_box";
  case role::radio_button:
    return "radio_button";
  case role::text_field:
    return "text_field";
  case role::slider:
    return "slider";
  case role::progress_bar:
    return "progress_bar";
  case role::scroll_area:
    return "scroll_area";
  case role::scroll_bar:
    return "scroll_bar";
  case role::splitter:
    return "splitter";
  case role::status_bar:
    return "status_bar";
  case role::menu_bar:
    return "menu_bar";
  case role::menu:
    return "menu";
  case role::menu_item:
    return "menu_item";
  case role::menu_item_check:
    return "menu_item_check";
  case role::menu_item_radio:
    return "menu_item_radio";
  case role::tab_list:
    return "tab_list";
  case role::tab:
    return "tab";
  case role::tab_panel:
    return "tab_panel";
  case role::list:
    return "list";
  case role::list_item:
    return "list_item";
  case role::tree:
    return "tree";
  case role::tree_item:
    return "tree_item";
  }
  return "unknown";
}

} // namespace basalt::accessibility
