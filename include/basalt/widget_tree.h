#pragma once

#include "basalt/file_dialog.h"
#include "basalt/focus.h"
#include "basalt/geometry.h"
#include "basalt/text/text_shaper.h"
#include "basalt/theme.h"
#include "basalt/units.h"
#include "basalt/widget.h"

#include <chrono>
#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

namespace basalt {

// Owns the root widget of one window and the state shared by every widget in
// it: the id registry, the theme and scale factors, keyboard focus, pending
// layout, and the damaged area awaiting repaint.
//
// A frame is driven by the platform: when the tree asks for a frame through
// the frame request handler, call `update_layout()`, paint the rects from
// `take_damage()`, then `collect_garbage()`.
//
// Widgets keep a pointer to their tree, so a tree can be neither copied nor
// moved.
class widget_tree {
public:
  widget_tree() = default;
  explicit widget_tree(basalt::theme theme, scale_factors scale = {});
  ~widget_tree();

  widget_tree(widget_tree const &) = delete;
  auto operator=(widget_tree const &) -> widget_tree & = delete;

  [[nodiscard]] auto root() const -> widget * { return root_.get(); }

  // Replaces the root, detaching and destroying the previous one, and sizes
  // the new root to the viewport. Throws `std::invalid_argument` when `root`
  // is null. Must not be called from the old root's lifecycle hooks.
  template <std::derived_from<widget> T> auto set_root(std::unique_ptr<T> root) -> T & {
    T *const added = root.get();
    replace_root(std::unique_ptr<widget>(std::move(root)));
    return *added;
  }
  template <std::derived_from<widget> T, class... Args> auto emplace_root(Args &&...args) -> T & {
    return set_root(std::make_unique<T>(std::forward<Args>(args)...));
  }
  // Detaches the root and hands back ownership; null when there is none.
  [[nodiscard]] auto take_root() -> std::unique_ptr<widget>;

  // The attached widget with `id`, or null.
  [[nodiscard]] auto find(widget_id id) const -> widget *;
  [[nodiscard]] auto contains(widget_id id) const -> bool { return find(id) != nullptr; }
  // Number of attached widgets.
  [[nodiscard]] auto size() const -> std::size_t { return registry_.size(); }

  // In layout pixels; the root always fills it.
  [[nodiscard]] auto viewport() const -> basalt::size { return viewport_; }
  auto set_viewport(basalt::size viewport) -> void;

  [[nodiscard]] auto theme() const -> basalt::theme const & { return theme_; }
  auto set_theme(basalt::theme theme) -> void;
  [[nodiscard]] auto scale() const -> scale_factors { return scale_; }
  // Ignores invalid factors and returns false.
  auto set_scale(scale_factors scale) -> bool;

  // Shapes the text of labels, buttons and fields. Not owned; null until the
  // application provides one, in which case text measures as zero width.
  [[nodiscard]] auto text_shaper() const -> text::text_shaper const * { return shaper_; }
  // Invalidates layout and paint, since every text measurement changes.
  auto set_text_shaper(text::text_shaper const *shaper) -> void;

  // The application's file chooser. Empty until the application provides one,
  // in which case widgets that need files cannot open a dialog.
  [[nodiscard]] auto file_dialog() const -> basalt::file_dialog const & { return file_dialog_; }
  auto set_file_dialog(basalt::file_dialog dialog) -> void { file_dialog_ = std::move(dialog); }

  [[nodiscard]] auto focus() -> focus_manager & { return focus_; }
  [[nodiscard]] auto focus() const -> focus_manager const & { return focus_; }

  [[nodiscard]] auto needs_layout() const -> bool;
  // Lays out every marked widget top-down. When `on_layout` keeps
  // invalidating ancestors, gives up after a bounded number of passes and
  // leaves the rest for the next frame. Returns true when nothing needs layout.
  auto update_layout() -> bool;

  // Damaged areas in root coordinates, clipped to the viewport and free of
  // rects contained in other rects.
  [[nodiscard]] auto damage() const -> std::span<rect const> { return damage_; }
  // Returns and clears the damage, completing the current frame request.
  [[nodiscard]] auto take_damage() -> std::vector<rect>;
  [[nodiscard]] auto needs_frame() const -> bool {
    return needs_layout() || !damage_.empty() || !animating_.empty();
  }
  // Runs `on_animation_frame` on the widgets that asked for it. Call once per
  // frame, before `update_layout()`, with a monotonic time.
  auto advance_animations(std::chrono::nanoseconds now) -> void;

  // Called once when the tree goes from idle to needing a frame, and again
  // only after `take_damage()` completes that frame.
  auto set_frame_request_handler(std::function<void()> handler) -> void;

  // Detaches `w` now and destroys it in `collect_garbage()`, so a widget can
  // remove itself from inside its own member functions. False when `w` is
  // not in this tree or is already being detached.
  auto destroy_later(widget &w) -> bool;
  auto collect_garbage() -> void;

  static constexpr std::size_t max_layout_passes = 8;
  // More damage rects than this are merged into their bounding rect.
  static constexpr std::size_t max_damage_rects = 16;

private:
  friend class focus_manager;
  friend class widget;

  auto replace_root(std::unique_ptr<widget> root) -> void;
  auto register_widget(widget &w) -> void;
  auto unregister_widget(widget &w) -> void;
  auto add_damage(rect area) -> void;
  auto request_frame() -> void;
  auto add_animation(widget &w) -> void;
  auto environment_changed() -> void;
  auto layout_subtree(widget &w) -> void;

  std::unique_ptr<widget> root_;
  std::unordered_map<widget_id, widget *> registry_;
  basalt::size viewport_;
  basalt::theme theme_;
  text::text_shaper const *shaper_ = nullptr;
  basalt::file_dialog file_dialog_;
  scale_factors scale_;
  std::vector<rect> damage_;
  std::vector<std::unique_ptr<widget>> graveyard_;
  std::vector<widget_id> animating_;
  std::optional<std::chrono::nanoseconds> last_animation_time_;
  std::function<void()> frame_request_handler_;
  bool frame_requested_ = false;
  focus_manager focus_{*this};
};

} // namespace basalt
