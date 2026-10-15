#pragma once

#include "basalt/widget.h"
#include "basalt/widgets/box.h"
#include "basalt/widgets/text_line.h"

#include <functional>
#include <optional>
#include <string>

namespace basalt {

class menu;

// One command in a menu: a label, an optional shortcut hint on the right and
// an optional check mark on the left. Activating it (click, Enter, Space or
// an assistive technology) calls the menu's activation handler with the
// item's id. Disabled items stay reachable by keyboard and do nothing.
class menu_item : public widget {
public:
  menu_item(std::string id, std::string label, std::string shortcut = {},
            std::optional<bool> checked = std::nullopt);

  [[nodiscard]] auto item_id() const -> std::string const & { return id_; }
  [[nodiscard]] auto label() const -> std::string const & { return label_.text(); }
  auto set_label(std::string label) -> void;
  [[nodiscard]] auto shortcut() const -> std::string const & { return shortcut_.text(); }
  auto set_shortcut(std::string shortcut) -> void;
  // Empty for plain items; otherwise the item shows a check column.
  [[nodiscard]] auto checked() const -> std::optional<bool> { return checked_; }
  auto set_checked(std::optional<bool> checked) -> void;

  // Called after the menu's handler. False when the item is disabled.
  auto activate() -> bool;
  auto set_on_activate(std::function<void()> handler) -> void { on_activate_ = std::move(handler); }

  [[nodiscard]] auto accessibility_info() const -> accessibility::info override;

protected:
  auto on_event(event_context &context, event const &e) -> bool override;
  auto on_paint(render_context &context) const -> void override;
  [[nodiscard]] auto on_measure(constraints const &limits) const -> basalt::size override;
  auto on_accessibility_action(accessibility::action_request const &request) -> bool override;

private:
  std::string id_;
  text_line label_;
  text_line shortcut_;
  std::optional<bool> checked_;
  std::function<void()> on_activate_;
};

// A vertical list of menu items and separators. The items form one focus
// group: Tab enters the menu at the item focused last, and Up, Down, Home and
// End move among the items, wrapping at the ends. Escape asks the owner to
// dismiss the menu. The menu draws its own panel; positioning it over other
// content and closing it afterwards belongs to whoever shows it.
class menu : public box {
public:
  explicit menu(std::string name = {});

  [[nodiscard]] auto name() const -> std::string const & { return name_; }
  auto set_name(std::string name) -> void { name_ = std::move(name); }

  auto add_item(std::string id, std::string label, std::string shortcut = {},
                std::optional<bool> checked = std::nullopt) -> menu_item &;
  auto add_separator() -> void;
  // The item with `id`, or null.
  [[nodiscard]] auto find_item(std::string const &id) const -> menu_item *;

  // Called with the id of an item that was activated.
  auto set_on_activate(std::function<void(std::string const &)> handler) -> void {
    on_activate_ = std::move(handler);
  }
  auto set_on_dismiss(std::function<void()> handler) -> void { on_dismiss_ = std::move(handler); }

  [[nodiscard]] auto accessibility_info() const -> accessibility::info override;

protected:
  auto on_event(event_context &context, event const &e) -> bool override;
  auto on_paint(render_context &context) const -> void override;

private:
  friend class menu_item;

  auto item_activated(menu_item const &item) -> void;

  std::string name_;
  std::function<void(std::string const &)> on_activate_;
  std::function<void()> on_dismiss_;
};

} // namespace basalt
