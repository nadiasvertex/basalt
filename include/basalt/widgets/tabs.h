#pragma once

#include "basalt/widget.h"
#include "basalt/widgets/box.h"
#include "basalt/widgets/text_line.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace basalt {

class tab_bar;
class overflow_button;

enum class tab_bar_style : std::uint8_t {
  // Tabs with an underline under the selected one; the default.
  line,
  // Equal-width tabs in a rounded group, for choosing among a few options.
  segmented,
};

// One tab of a `tab_bar`. Clicking a tab or moving keyboard focus to it
// selects it.
class tab : public widget {
public:
  explicit tab(std::string label = {});

  [[nodiscard]] auto label() const -> std::string const & { return line_.text(); }
  auto set_label(std::string label) -> void;
  [[nodiscard]] auto selected() const -> bool { return selected_; }
  auto set_selected(bool selected) -> void;
  // A closable tab shows a close button at its trailing edge. Pressing it asks
  // the owning `tab_bar` to report the close; the tab itself is not removed.
  [[nodiscard]] auto closable() const -> bool { return closable_; }
  auto set_closable(bool closable) -> void;

  [[nodiscard]] auto accessibility_info() const -> accessibility::info override;

protected:
  auto on_event(event_context &context, event const &e) -> bool override;
  auto on_paint(render_context &context) const -> void override;
  [[nodiscard]] auto on_measure(constraints const &limits) const -> basalt::size override;
  auto on_interaction_changed(interaction_state const &previous) -> void override;
  auto on_accessibility_action(accessibility::action_request const &request) -> bool override;

private:
  auto request_selection() -> void;
  auto request_close() -> void;
  [[nodiscard]] auto close_area() const -> rect;
  [[nodiscard]] auto over_close(point local) const -> bool;
  // The label, center-truncated with an ellipsis when it is wider than `available`.
  [[nodiscard]] auto fitted_label(float available) const -> text_line const &;

  text_line line_;
  // The label shortened to fit the text area; reused while the label and width are unchanged.
  mutable text_line fitted_line_;
  mutable float fitted_width_ = -1;
  mutable float fitted_natural_ = -1;
  mutable std::string fitted_source_;
  bool selected_ = false;
  bool closable_ = false;
  bool close_hovered_ = false;
  bool close_pressed_ = false;

  friend class tab_bar;
  [[nodiscard]] auto segmented() const -> bool;
};

// A row of tabs. The tabs are one focus group, so Tab enters at the selected
// tab and Left, Right, Home and End move among them, selecting as they go.
// The bar reports the selection; showing the matching content is up to the
// owner.
//
// Tabs shrink toward a minimum width as the row fills. When they no longer fit,
// the row scrolls horizontally: the wheel or trackpad moves it (a vertical wheel
// is turned sideways), and selecting a tab scrolls it into view with a little of
// its neighbours showing. A bar that scrolls also shows a fade at each edge that
// has tabs past it, a scroll button at each end and, when an `overlay_host` is
// above it, a button that opens a menu of every tab. A scroll button moves just
// far enough to show the next partly hidden tab, and repeats while held. The
// buttons are not Tab stops: the arrow keys scroll as they select, and Down on a
// tab opens the menu. The buttons paint the bar's background, so the bar should
// sit on that color (see `set_background`).
//
// The `segmented` style divides the width equally among the tabs and never
// scrolls.
class tab_bar : public box {
public:
  explicit tab_bar(std::string name = {});

  [[nodiscard]] auto name() const -> std::string const & { return name_; }
  auto set_name(std::string name) -> void { name_ = std::move(name); }

  auto add_tab(std::string label) -> tab &;
  [[nodiscard]] auto tab_count() const -> std::size_t;
  [[nodiscard]] auto tab_at(std::size_t index) const -> tab &;
  // Empty when no tab is selected.
  [[nodiscard]] auto selected_index() const -> std::optional<std::size_t>;
  // Selects the tab, deselecting the others, and calls the handler when the
  // selection changed. False when `index` is out of range.
  auto select(std::size_t index) -> bool;

  // Called with the index of a tab whose close button was pressed. The handler
  // may remove or rebuild the tabs.
  auto set_on_close(std::function<void(std::size_t)> handler) -> void {
    on_close_ = std::move(handler);
  }

  // Called with the index of the newly selected tab.
  auto set_on_select(std::function<void(std::size_t)> handler) -> void {
    on_select_ = std::move(handler);
  }

  // How far the row is scrolled, in layout pixels; zero when the tabs fit.
  [[nodiscard]] auto scroll_offset() const -> float { return offset_; }
  [[nodiscard]] auto max_scroll_offset() const -> float;
  // Returns true when the offset changed.
  auto set_scroll_offset(float offset) -> bool;
  // Scrolls as little as needed to show the tab, with room for its neighbours.
  auto scroll_tab_into_view(std::size_t index) -> void;

  // True while the tabs do not fit and the overflow controls are shown.
  [[nodiscard]] auto overflowing() const -> bool { return overflowing_; }
  // Scroll just far enough to show the next partly hidden tab. True when the
  // offset changed.
  auto scroll_toward_start() -> bool;
  auto scroll_toward_end() -> bool;

  // Opens the menu of every tab. False when the bar is not overflowing, there
  // is no `overlay_host` above it or the menu is already open.
  auto open_tab_menu() -> bool;
  [[nodiscard]] auto tab_menu_open() const -> bool;

  [[nodiscard]] auto style() const -> tab_bar_style { return style_; }
  auto set_style(tab_bar_style style) -> void;

  // The color the overflow controls paint and the edge fades blend into.
  auto set_background(rgba tokens::color_set::*color) -> void;

  [[nodiscard]] auto accessibility_info() const -> accessibility::info override;

protected:
  auto on_event(event_context &context, event const &e) -> bool override;
  auto on_layout() -> void override;
  auto on_attached() -> void override;
  auto on_child_removed(widget &child) -> void override;
  // An empty bar keeps the height of a tab so the tray stays in place.
  [[nodiscard]] auto on_measure(constraints const &limits) const -> basalt::size override;
  auto on_paint(render_context &context) const -> void override;
  auto on_paint_overlay(render_context &context) const -> void override;

private:
  friend class tab;

  // Moves the tabs by `delta` without a new layout.
  auto shift_tabs(float delta) -> void;
  auto ensure_controls() -> void;
  [[nodiscard]] auto tab_item() const -> box_item;
  // The width of the row between the overflow controls.
  [[nodiscard]] auto view_width() const -> float;
  [[nodiscard]] auto content_left(std::size_t index) const -> float;
  [[nodiscard]] auto content_right(std::size_t index) const -> float;
  auto place_controls() -> void;
  auto close_tab_menu() -> void;

  std::string name_;
  float offset_ = 0;
  float content_width_ = 0;
  bool reveal_selected_ = false;
  bool overflowing_ = false;
  // Width reserved for the controls at each end while overflowing.
  float lead_ = 0;
  float trail_ = 0;
  tab_bar_style style_ = tab_bar_style::line;
  rgba tokens::color_set::*background_ = &tokens::color_set::background_default;
  // Scroll back, scroll forward and the tab menu; null when the bar was cleared.
  std::array<overflow_button *, 3> controls_{};
  widget_id menu_popup_;
  std::function<void(std::size_t)> on_select_;
  std::function<void(std::size_t)> on_close_;
};

} // namespace basalt
