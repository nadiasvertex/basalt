#include "basalt/widgets/accordion.h"

#include "basalt/event_router.h"
#include "basalt/widget_tree.h"

#include <algorithm>
#include <variant>

#include "glyphs.h"

namespace basalt {

namespace a11y = accessibility;

// The clickable row of an item. It owns the actions widget, when there is one.
class accordion_header : public widget {
public:
  explicit accordion_header(accordion_item &owner) : owner_(owner) {
    set_focus_policy(focus_policy::strong);
    set_minimum_target(true);
  }

  auto set_actions(std::unique_ptr<widget> actions) -> void {
    if (actions_ != nullptr) {
      (void)remove_child(*actions_);
      actions_ = nullptr;
    }
    if (actions != nullptr) {
      actions_ = &add_child(std::move(actions));
    }
    invalidate_layout();
  }

  [[nodiscard]] auto accessibility_info() const -> a11y::info override {
    a11y::info details;
    details.kind = a11y::role::button;
    details.name = owner_.heading();
    details.expanded = owner_.expanded();
    details.actions = {a11y::action::press, a11y::action::focus};
    return details;
  }

protected:
  auto on_event(event_context &, event const &e) -> bool override {
    if (auto const *p = std::get_if<pointer_event>(&e)) {
      if (p->button != pointer_button::primary) {
        return false;
      }
      if (p->action == pointer_action::down) {
        return true;
      }
      if (p->action == pointer_action::up) {
        if (interaction().pressed) {
          owner_.toggle();
        }
        return true;
      }
      return false;
    }
    if (auto const *k = std::get_if<key_event>(&e)) {
      if (k->pressed && !k->repeat && k->modifiers == modifier::none &&
          (k->code == key::enter || k->code == key::space)) {
        owner_.toggle();
        return true;
      }
    }
    return false;
  }

  auto on_accessibility_action(a11y::action_request const &request) -> bool override {
    if (request.kind != a11y::action::press || !enabled_in_tree()) {
      return false;
    }
    owner_.toggle();
    return true;
  }

  [[nodiscard]] auto on_measure(constraints const &limits) const -> basalt::size override {
    basalt::theme const &t = theme();
    basalt::size const text = owner_.heading_.measure(*this);
    float const inset = resolve(t.sizes.inset_horizontal_accordion_header);
    float const mark = resolve(t.sizes.dimension_icon);
    basalt::size const actions = actions_ != nullptr ? actions_->measure(limits) : basalt::size{};
    return {2 * inset + mark + inset + text.width + (actions.width > 0 ? inset : 0) + actions.width,
            std::max({text.height + 2 * resolve(t.sizes.inset_vertical_list_item_text),
                      resolve(t.sizes.dimension_height_accordion_header), actions.height})};
  }

  auto on_layout() -> void override {
    if (actions_ == nullptr) {
      return;
    }
    basalt::theme const &t = theme();
    float const inset = resolve(t.sizes.inset_horizontal_accordion_header);
    basalt::size const wanted = actions_->measure(constraints::loose(bounds().extent));
    actions_->set_bounds(
        {{bounds().width() - inset - wanted.width, (bounds().height() - wanted.height) / 2},
         wanted});
  }

  auto on_paint(render_context &context) const -> void override {
    if (tree() == nullptr) {
      return;
    }
    basalt::theme const &t = theme();
    rect const area{{}, bounds().extent};
    float const stroke = device_stroke_width(fixed(1), scale());
    switch (control_state_of(interaction(), enabled())) {
    case control_state::hovered:
      context.fill_rect(area, t.colors.background_accent_transparent_hover);
      break;
    case control_state::pressed:
      context.fill_rect(area, t.colors.background_accent_transparent_active);
      break;
    case control_state::normal:
      if (owner_.appearance_ == accordion_appearance::filled) {
        context.fill_rect(area, t.colors.background_secondary);
      }
      break;
    case control_state::disabled:
      break;
    }
    float const inset = resolve(t.sizes.inset_horizontal_accordion_header);
    float const mark = resolve(t.sizes.dimension_icon);
    bool const at_start = owner_.chevron_ == chevron_placement::start;
    float const actions_width = actions_ != nullptr ? actions_->bounds().width() + inset : 0;
    float const chevron_left = at_start ? inset : area.width() - inset - mark - actions_width;
    float const text_left = at_start ? inset + mark + inset : inset;
    float const text_right = at_start ? area.width() - actions_width - inset : chevron_left - inset;
    rect const chevron_area{{chevron_left, (area.height() - mark) / 2}, {mark, mark}};
    glyphs::draw_chevron(context, chevron_area, owner_.expanded(), t.colors.icon_default,
                         device_stroke_width(fixed(1.5f), scale()));
    owner_.heading_.draw_in(
        context, *this, {{text_left, 0}, {std::max(0.0f, text_right - text_left), area.height()}},
        t.colors.text_default);
    if (owner_.appearance_ == accordion_appearance::plain) {
      context.line({0, area.height() - stroke / 2}, {area.width(), area.height() - stroke / 2},
                   t.colors.border_subdued, stroke);
    }
  }

private:
  accordion_item &owner_;
  widget *actions_ = nullptr;
};

accordion_item::accordion_item(std::string heading, control_size size)
    : box(axis::vertical), heading_(std::move(heading), text_role::body_strong), size_(size) {
  set_cross_alignment(alignment::stretch);
  header_ = &emplace_child<accordion_header>(*this);
  heading_.set_role(size == control_size::small   ? text_role::small_strong
                    : size == control_size::large ? text_role::large
                                                  : text_role::body_strong);
}

accordion_item::~accordion_item() = default;

auto accordion_item::header() const -> widget & { return *header_; }

auto accordion_item::set_heading(std::string heading) -> void {
  if (heading_.set_text(std::move(heading))) {
    header_->invalidate_content();
  }
}

auto accordion_item::content() const -> widget * { return child_count() > 1 ? &child(1) : nullptr; }

auto accordion_item::replace_content(std::unique_ptr<widget> content) -> void {
  if (widget *const old = this->content()) {
    (void)remove_child(*old);
  }
  if (content != nullptr) {
    content->set_visible(expanded_);
    add_child(std::move(content));
  }
}

auto accordion_item::replace_actions(std::unique_ptr<widget> actions) -> void {
  header_->set_actions(std::move(actions));
}

auto accordion_item::set_expanded(bool expanded) -> void {
  if (expanded == expanded_) {
    return;
  }
  expanded_ = expanded;
  if (widget *const body = content()) {
    body->set_visible(expanded);
  }
  header_->invalidate_paint();
  invalidate_layout();
}

auto accordion_item::toggle() -> void {
  if (!enabled_in_tree()) {
    return;
  }
  set_expanded(!expanded_);
  user_toggled();
}

auto accordion_item::user_toggled() -> void {
  if (auto *const owner = parent_as<accordion>(); owner != nullptr && expanded_) {
    owner->item_expanded(*this);
  }
  if (on_toggle_) {
    auto const handler = on_toggle_;
    auto const state = expanded_;
    handler(state);
  }
}

auto accordion_item::set_size(control_size size) -> void {
  size_ = size;
  heading_.set_role(size == control_size::small   ? text_role::small_strong
                    : size == control_size::large ? text_role::large
                                                  : text_role::body_strong);
  header_->invalidate_content();
}

auto accordion_item::set_appearance(accordion_appearance appearance) -> void {
  appearance_ = appearance;
  header_->invalidate_paint();
  invalidate_paint();
}

auto accordion_item::set_chevron(chevron_placement placement) -> void {
  chevron_ = placement;
  header_->invalidate_paint();
}

accordion::accordion(std::string name) : box(axis::vertical), name_(std::move(name)) {
  set_cross_alignment(alignment::stretch);
}

auto accordion::add_item(std::string heading, std::unique_ptr<widget> content) -> accordion_item & {
  auto &added = emplace_child<accordion_item>(std::move(heading), size_);
  added.set_appearance(appearance_);
  added.set_chevron(chevron_);
  if (content != nullptr) {
    added.set_content(std::move(content));
  }
  return added;
}

auto accordion::item(std::size_t index) const -> accordion_item & {
  return static_cast<accordion_item &>(child(index));
}

auto accordion::expand_all() -> void {
  if (exclusive_) {
    return;
  }
  for (std::size_t i = 0; i < item_count(); ++i) {
    item(i).set_expanded(true);
  }
}

auto accordion::collapse_all() -> void {
  for (std::size_t i = 0; i < item_count(); ++i) {
    item(i).set_expanded(false);
  }
}

auto accordion::set_exclusive(bool exclusive) -> void {
  exclusive_ = exclusive;
  if (!exclusive_) {
    return;
  }
  bool seen = false;
  for (std::size_t i = 0; i < item_count(); ++i) {
    if (item(i).expanded()) {
      if (seen) {
        item(i).set_expanded(false);
      }
      seen = true;
    }
  }
}

auto accordion::set_appearance(accordion_appearance appearance) -> void {
  appearance_ = appearance;
  for (std::size_t i = 0; i < item_count(); ++i) {
    item(i).set_appearance(appearance);
  }
  invalidate_paint();
}

auto accordion::set_chevron(chevron_placement placement) -> void {
  chevron_ = placement;
  for (std::size_t i = 0; i < item_count(); ++i) {
    item(i).set_chevron(placement);
  }
}

auto accordion::set_size(control_size size) -> void {
  size_ = size;
  for (std::size_t i = 0; i < item_count(); ++i) {
    item(i).set_size(size);
  }
}

auto accordion::item_expanded(accordion_item const &opened) -> void {
  if (!exclusive_) {
    return;
  }
  for (std::size_t i = 0; i < item_count(); ++i) {
    if (&item(i) != &opened) {
      item(i).set_expanded(false);
    }
  }
}

auto accordion::accessibility_info() const -> a11y::info {
  a11y::info details;
  details.kind = a11y::role::group;
  details.name = name_;
  return details;
}

} // namespace basalt
