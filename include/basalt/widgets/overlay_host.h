#pragma once

#include "basalt/geometry.h"
#include "basalt/widget.h"
#include "basalt/widget_id.h"

#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace basalt {

// The root of a window's widget tree: one content widget that fills the
// window, with popups and dialogs shown above it.
//
// Children are clipped to their parent, so anything that must extend past the
// widget that opens it, such as a menu, is shown here instead. Each overlay
// sits in a layer that covers the window, blocks the pointer from reaching the
// content beneath and traps keyboard focus (see `focus_manager::push_modal`)
// until it is dismissed. Overlays stack: the most recent one is on top.
//
// A popup is placed below an anchor rectangle, or above it when it does not
// fit, and is dismissed by a press outside it or by Escape. A dialog is
// centered over a dimmed window and is dismissed by its owner or by Escape.
// The `on_close` handler runs once when an overlay goes away, whichever way.
class overlay_host : public widget {
public:
  overlay_host();
  ~overlay_host() override;

  template <std::derived_from<widget> T> auto set_content(std::unique_ptr<T> content) -> T & {
    T *const added = content.get();
    replace_content(std::unique_ptr<widget>(std::move(content)));
    return *added;
  }
  // Null when there is no content.
  [[nodiscard]] auto content() const -> widget *;

  // `anchor` is in this host's coordinates. Returns the id of the overlay,
  // which `dismiss` accepts.
  auto show_popup(std::unique_ptr<widget> popup, rect anchor, std::function<void()> on_close = {})
      -> widget_id;
  // Like `show_popup`, but non-modal: the pointer passes through to the
  // content, focus stays where it is, and only `dismiss` or `dismiss_all`
  // closes it. For tooltips and hover popovers.
  auto show_hover_popup(std::unique_ptr<widget> popup, rect anchor,
                        std::function<void()> on_close = {}) -> widget_id;
  auto show_dialog(std::unique_ptr<widget> dialog, std::function<void()> on_close = {})
      -> widget_id;

  // False when the overlay is already gone.
  auto dismiss(widget_id overlay) -> bool;
  auto dismiss_all() -> void;
  [[nodiscard]] auto overlay_count() const -> std::size_t { return overlays_.size(); }
  [[nodiscard]] auto is_open(widget_id overlay) const -> bool;

  // The nearest host above `w`, or null.
  [[nodiscard]] static auto find(widget const &w) -> overlay_host *;

protected:
  auto on_layout() -> void override;
  [[nodiscard]] auto on_measure(constraints const &limits) const -> basalt::size override;
  auto on_child_removed(widget &child) -> void override;

private:
  friend class overlay_layer;

  auto replace_content(std::unique_ptr<widget> content) -> void;
  auto add_layer(std::unique_ptr<widget> shown, bool dialog, bool hover, rect anchor,
                 std::function<void()> on_close) -> widget_id;

  widget_id content_id_;
  std::vector<widget_id> overlays_;
  std::vector<widget_id> hover_overlays_;
};

} // namespace basalt
