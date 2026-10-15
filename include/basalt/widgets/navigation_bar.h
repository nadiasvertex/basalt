#pragma once

#include "basalt/widget.h"
#include "basalt/widgets/avatar.h"
#include "basalt/widgets/box.h"
#include "basalt/widgets/label.h"
#include "basalt/widgets/text_line.h"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace basalt {

class navigation_bar;

// One destination in a navigation bar. It has default, hover and selected
// appearances. With entries it opens them as a menu below itself instead of
// navigating.
class navigation_item : public widget {
public:
  navigation_item(std::string id, std::string label);
  ~navigation_item() override;

  [[nodiscard]] auto item_id() const -> std::string const & { return id_; }
  [[nodiscard]] auto label() const -> std::string const & { return label_.text(); }
  auto set_label(std::string label) -> void;
  [[nodiscard]] auto selected() const -> bool { return selected_; }
  auto set_selected(bool selected) -> void;

  // Adds an entry to the item's submenu.
  auto add_entry(std::string id, std::string label) -> void;
  [[nodiscard]] auto entry_count() const -> std::size_t { return entries_.size(); }
  [[nodiscard]] auto is_open() const -> bool;
  auto close() -> void;

  // Navigates, or opens the submenu. False when disabled.
  auto activate() -> bool;

  [[nodiscard]] auto accessibility_info() const -> accessibility::info override;

protected:
  auto on_event(event_context &context, event const &e) -> bool override;
  auto on_paint(render_context &context) const -> void override;
  [[nodiscard]] auto on_measure(constraints const &limits) const -> basalt::size override;
  auto on_detached() -> void override;
  auto on_accessibility_action(accessibility::action_request const &request) -> bool override;

private:
  struct entry {
    std::string id;
    std::string label;
  };

  auto open_entries() -> bool;

  std::string id_;
  text_line label_;
  std::vector<entry> entries_;
  bool selected_ = false;
  widget_id popup_;
};

enum class navigation_layout : std::uint8_t {
  // Title, sections and utility links.
  sections,
  // Title and actions only.
  toolbar,
};

// A bar across the top of an application. The sections layout shows leading
// actions, a title, navigation sections, center actions, utility links and an
// avatar. The toolbar layout drops the sections and utility links. Choosing a
// section, an entry of a section's submenu or a utility link calls the
// navigation handler with its id; sections and their entries also become the
// selected destination.
class navigation_bar : public box {
public:
  explicit navigation_bar(std::string name = {});

  [[nodiscard]] auto name() const -> std::string const & { return name_; }
  auto set_name(std::string name) -> void { name_ = std::move(name); }
  [[nodiscard]] auto layout() const -> navigation_layout { return layout_; }
  auto set_layout(navigation_layout layout) -> void;

  [[nodiscard]] auto title() const -> std::string const & { return title_->text(); }
  auto set_title(std::string title) -> void;

  auto add_section(std::string id, std::string label) -> navigation_item &;
  auto add_utility_link(std::string id, std::string label) -> navigation_item &;
  // The section or utility link with `id`, or null.
  [[nodiscard]] auto find_item(std::string const &id) const -> navigation_item *;

  template <std::derived_from<widget> T, class... Args>
  auto emplace_leading(Args &&...args) -> T & {
    return leading_->emplace_child<T>(std::forward<Args>(args)...);
  }
  template <std::derived_from<widget> T, class... Args> auto emplace_center(Args &&...args) -> T & {
    return center_->emplace_child<T>(std::forward<Args>(args)...);
  }
  // Replaces the avatar at the end of the bar.
  auto set_avatar(std::unique_ptr<avatar> person) -> avatar &;

  // The selected destination, or empty. Does not call the handler.
  [[nodiscard]] auto selected_id() const -> std::optional<std::string> { return selected_; }
  auto select(std::optional<std::string> id) -> void;
  auto set_on_navigate(std::function<void(std::string const &)> handler) -> void {
    on_navigate_ = std::move(handler);
  }

  [[nodiscard]] auto accessibility_info() const -> accessibility::info override;

private:
  friend class navigation_item;

  auto chosen(navigation_item const &from, std::string const &id) -> void;

  std::string name_;
  navigation_layout layout_ = navigation_layout::sections;
  box *leading_ = nullptr;
  label *title_ = nullptr;
  box *sections_ = nullptr;
  box *center_ = nullptr;
  box *utility_ = nullptr;
  avatar *avatar_ = nullptr;
  std::optional<std::string> selected_;
  std::function<void(std::string const &)> on_navigate_;
};

} // namespace basalt
