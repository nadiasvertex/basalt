#pragma once

#include "basalt/widget.h"
#include "basalt/widgets/box.h"
#include "basalt/widgets/control_size.h"
#include "basalt/widgets/text_line.h"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace basalt {

enum class accordion_appearance : std::uint8_t {
  // A rule between items.
  plain,
  // Each item has a border.
  outlined,
  // Each header has a subdued background.
  filled,
};

enum class chevron_placement : std::uint8_t { start, end };

class accordion;
class accordion_header;

// One section of an accordion: a header with a heading and a chevron, and
// content that is shown while the item is expanded. The header is a button
// that Enter, Space or a click toggles. Optional actions sit at the end of the
// header and handle their own input.
class accordion_item : public box {
public:
  explicit accordion_item(std::string heading, control_size size = control_size::medium);
  ~accordion_item() override;

  [[nodiscard]] auto heading() const -> std::string const & { return heading_.text(); }
  auto set_heading(std::string heading) -> void;

  template <std::derived_from<widget> T> auto set_content(std::unique_ptr<T> content) -> T & {
    T &added = *content;
    replace_content(std::unique_ptr<widget>(std::move(content)));
    return added;
  }
  template <std::derived_from<widget> T, class... Args>
  auto emplace_content(Args &&...args) -> T & {
    return set_content(std::make_unique<T>(std::forward<Args>(args)...));
  }
  // Null when there is no content.
  [[nodiscard]] auto content() const -> widget *;

  // Shown at the end of the header.
  template <std::derived_from<widget> T> auto set_actions(std::unique_ptr<T> actions) -> T & {
    T &added = *actions;
    replace_actions(std::unique_ptr<widget>(std::move(actions)));
    return added;
  }

  [[nodiscard]] auto expanded() const -> bool { return expanded_; }
  // Does not call the toggle handler.
  auto set_expanded(bool expanded) -> void;
  // Does what a click on the header does.
  auto toggle() -> void;
  // Called with the new state after the user toggles the item.
  auto set_on_toggle(std::function<void(bool)> handler) -> void { on_toggle_ = std::move(handler); }

  [[nodiscard]] auto header() const -> widget &;

private:
  friend class accordion;
  friend class accordion_header;

  auto replace_content(std::unique_ptr<widget> content) -> void;
  auto replace_actions(std::unique_ptr<widget> actions) -> void;
  auto set_size(control_size size) -> void;
  auto set_appearance(accordion_appearance appearance) -> void;
  auto set_chevron(chevron_placement placement) -> void;
  auto user_toggled() -> void;

  text_line heading_;
  control_size size_;
  accordion_appearance appearance_ = accordion_appearance::plain;
  chevron_placement chevron_ = chevron_placement::start;
  bool expanded_ = false;
  accordion_header *header_ = nullptr;
  std::function<void(bool)> on_toggle_;
};

// A column of expandable items. By default any number can be open; with
// `set_exclusive(true)` opening one closes the others.
class accordion : public box {
public:
  explicit accordion(std::string name = {});

  [[nodiscard]] auto name() const -> std::string const & { return name_; }
  auto set_name(std::string name) -> void { name_ = std::move(name); }

  // Adds an item and returns it. `content` may be null and set later.
  auto add_item(std::string heading, std::unique_ptr<widget> content = nullptr) -> accordion_item &;
  [[nodiscard]] auto item_count() const -> std::size_t { return child_count(); }
  // Precondition: `index < item_count()`.
  [[nodiscard]] auto item(std::size_t index) const -> accordion_item &;

  auto expand_all() -> void;
  auto collapse_all() -> void;

  [[nodiscard]] auto exclusive() const -> bool { return exclusive_; }
  auto set_exclusive(bool exclusive) -> void;
  [[nodiscard]] auto appearance() const -> accordion_appearance { return appearance_; }
  auto set_appearance(accordion_appearance appearance) -> void;
  [[nodiscard]] auto chevron() const -> chevron_placement { return chevron_; }
  auto set_chevron(chevron_placement placement) -> void;
  [[nodiscard]] auto size() const -> control_size { return size_; }
  auto set_size(control_size size) -> void;

  [[nodiscard]] auto accessibility_info() const -> accessibility::info override;

private:
  friend class accordion_item;

  auto item_expanded(accordion_item const &item) -> void;

  std::string name_;
  bool exclusive_ = false;
  accordion_appearance appearance_ = accordion_appearance::plain;
  chevron_placement chevron_ = chevron_placement::start;
  control_size size_ = control_size::medium;
};

} // namespace basalt
