#include "basalt/widgets/overlay_host.h"

#include "basalt/event_router.h"
#include "basalt/render_context.h"
#include "basalt/widget_tree.h"

#include <algorithm>
#include <utility>
#include <variant>

namespace basalt {

// One overlay: covers the host, swallows pointer input outside the shown
// widget and places that widget.
class overlay_layer final : public widget {
public:
  overlay_layer(overlay_host &host, bool dialog, bool hover, rect anchor,
                std::function<void()> on_close)
      : host_(&host), dialog_(dialog), anchor_(anchor), on_close_(std::move(on_close)) {
    set_focus_policy(focus_policy::none);
    // A hover popup lets the pointer through to the content beneath it.
    set_hit_testable(!hover);
  }

  auto shown() const -> widget * { return child_count() > 0 ? &child(0) : nullptr; }

  // Runs the close handler once; the host calls this when it removes the layer.
  auto finish() -> void {
    auto const handler = std::exchange(on_close_, {});
    if (handler) {
      handler();
    }
  }

protected:
  auto on_event(event_context &, event const &e) -> bool override {
    if (auto const *p = std::get_if<pointer_event>(&e)) {
      // Only presses outside the shown widget reach the layer itself.
      if (p->action == pointer_action::down && !dialog_) {
        host_->dismiss(id());
      }
      return true;
    }
    if (std::holds_alternative<wheel_event>(e)) {
      return true;
    }
    if (auto const *k = std::get_if<key_event>(&e)) {
      if (k->pressed && k->code == key::escape && k->modifiers == modifier::none) {
        host_->dismiss(id());
        return true;
      }
    }
    return false;
  }

  auto on_paint(render_context &context) const -> void override {
    if (dialog_ && tree() != nullptr) {
      context.fill_rect({{}, bounds().extent}, tree()->theme().colors.background_overlay);
    }
  }

  auto on_layout() -> void override {
    widget *const content = shown();
    if (content == nullptr) {
      return;
    }
    basalt::size const area = bounds().extent;
    basalt::size wanted = content->measure(constraints::loose(area));
    if (dialog_ && tree() != nullptr) {
      wanted.width =
          std::max(wanted.width, resolve(tree()->theme().sizes.dimension_width_dialog_min));
      wanted.width = std::min(wanted.width, area.width);
    }
    point origin;
    if (dialog_) {
      origin = {(area.width - wanted.width) / 2, (area.height - wanted.height) / 3};
    } else {
      origin = {std::clamp(anchor_.left(), 0.0f, std::max(0.0f, area.width - wanted.width)),
                anchor_.bottom()};
      if (origin.y + wanted.height > area.height) {
        origin.y = std::max(0.0f, anchor_.top() - wanted.height);
      }
    }
    content->set_bounds({origin, wanted});
  }

private:
  overlay_host *host_;
  bool dialog_;
  rect anchor_;
  std::function<void()> on_close_;
};

overlay_host::overlay_host() = default;
overlay_host::~overlay_host() = default;

auto overlay_host::content() const -> widget * {
  for (widget &c : children()) {
    if (c.id() == content_id_) {
      return &c;
    }
  }
  return nullptr;
}

auto overlay_host::replace_content(std::unique_ptr<widget> content) -> void {
  if (widget *const old = this->content()) {
    (void)remove_child(*old);
  }
  content_id_ = content->id();
  // The content sits behind every overlay.
  (void)insert_child(0, std::move(content));
  invalidate_layout();
}

auto overlay_host::on_measure(constraints const &limits) const -> basalt::size {
  widget const *const c = content();
  return c != nullptr ? c->measure(limits) : limits.min;
}

auto overlay_host::on_layout() -> void {
  rect const area{{}, bounds().extent};
  for (widget &c : children()) {
    c.set_bounds(area);
  }
}

auto overlay_host::on_child_removed(widget &child) -> void {
  if (child.id() == content_id_) {
    content_id_ = {};
  }
}

auto overlay_host::add_layer(std::unique_ptr<widget> shown, bool dialog, bool hover, rect anchor,
                             std::function<void()> on_close) -> widget_id {
  auto layer = std::make_unique<overlay_layer>(*this, dialog, hover, anchor, std::move(on_close));
  layer->add_child(std::move(shown));
  overlay_layer &added = add_child(std::move(layer));
  overlays_.push_back(added.id());
  if (hover) {
    hover_overlays_.push_back(added.id());
  } else if (tree() != nullptr) {
    tree()->focus().push_modal(added);
  }
  invalidate_layout();
  return added.id();
}

auto overlay_host::show_popup(std::unique_ptr<widget> popup, rect anchor,
                              std::function<void()> on_close) -> widget_id {
  return add_layer(std::move(popup), false, false, anchor, std::move(on_close));
}

auto overlay_host::show_hover_popup(std::unique_ptr<widget> popup, rect anchor,
                                    std::function<void()> on_close) -> widget_id {
  return add_layer(std::move(popup), false, true, anchor, std::move(on_close));
}

auto overlay_host::show_dialog(std::unique_ptr<widget> dialog, std::function<void()> on_close)
    -> widget_id {
  return add_layer(std::move(dialog), true, false, {}, std::move(on_close));
}

auto overlay_host::is_open(widget_id overlay) const -> bool {
  return std::ranges::find(overlays_, overlay) != overlays_.end();
}

auto overlay_host::dismiss(widget_id overlay) -> bool {
  auto const it = std::ranges::find(overlays_, overlay);
  if (it == overlays_.end() || tree() == nullptr) {
    return false;
  }
  overlays_.erase(it);
  widget *const found = tree()->find(overlay);
  if (found == nullptr) {
    return false;
  }
  if (auto const hover = std::ranges::find(hover_overlays_, overlay);
      hover != hover_overlays_.end()) {
    hover_overlays_.erase(hover);
  } else {
    tree()->focus().pop_modal(*found);
  }
  auto &layer = static_cast<overlay_layer &>(*found);
  layer.finish();
  tree()->destroy_later(layer);
  return true;
}

auto overlay_host::dismiss_all() -> void {
  if (tree() == nullptr) {
    overlays_.clear();
    hover_overlays_.clear();
    return;
  }
  while (!overlays_.empty()) {
    dismiss(overlays_.back());
  }
}

auto overlay_host::find(widget const &w) -> overlay_host * {
  for (widget *p = w.parent(); p != nullptr; p = p->parent()) {
    if (auto *const host = dynamic_cast<overlay_host *>(p)) {
      return host;
    }
  }
  return nullptr;
}

} // namespace basalt
