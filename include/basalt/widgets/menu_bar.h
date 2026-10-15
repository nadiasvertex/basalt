#pragma once

#include "basalt/widget.h"
#include "basalt/widget_id.h"
#include "basalt/widgets/box.h"
#include "basalt/widgets/menu.h"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace basalt {

// A row of menu titles along the top of a window. Choosing a title opens its
// menu in the nearest `overlay_host` below the title. The titles form one
// focus group: Tab enters the bar, Left and Right move among the titles, and
// Enter or Space opens the focused one.
//
// A menu is built each time it opens, by the function given to `add_menu`, so
// enabled and checked state is always current. Activating an item closes the
// menu and then calls the command handler with the item's id.
class menu_bar : public box {
public:
  using populate_function = std::function<void(menu &)>;

  explicit menu_bar(std::string name = "Menu bar");

  auto add_menu(std::string title, populate_function populate) -> void;
  [[nodiscard]] auto menu_count() const -> std::size_t { return menus_.size(); }
  // Hides or shows the title of the menu at `index`. A hidden menu is skipped by layout and focus.
  // Closes the open menu first when it is the one being hidden.
  auto set_menu_visible(std::size_t index, bool visible) -> void;
  auto set_on_command(std::function<void(std::string const &)> handler) -> void {
    on_command_ = std::move(handler);
  }

  // Opens the menu at `index`, replacing an open one. False when the index is
  // out of range or the bar is not inside an overlay host.
  auto open_menu(std::size_t index) -> bool;
  auto close_menu() -> void;
  [[nodiscard]] auto is_open() const -> bool;

  [[nodiscard]] auto name() const -> std::string const & { return name_; }
  [[nodiscard]] auto accessibility_info() const -> accessibility::info override;

protected:
  auto on_event(event_context &context, event const &e) -> bool override;
  auto on_paint(render_context &context) const -> void override;

private:
  struct entry {
    std::string title;
    populate_function populate;
  };

  std::string name_;
  std::vector<entry> menus_;
  std::function<void(std::string const &)> on_command_;
  widget_id open_popup_;
};

} // namespace basalt
