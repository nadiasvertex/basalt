#include "basalt/widgets/tree_view.h"

#include "basalt/event_router.h"
#include "basalt/render_context.h"
#include "basalt/widget_tree.h"
#include "basalt/widgets/text_line.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <ranges>
#include <utility>
#include <variant>
#include <vector>

#include "glyphs.h"
#include "notify.h"

namespace basalt {

namespace {

// Typing after this pause starts a new search.
constexpr std::chrono::milliseconds type_ahead_pause{1000};

auto lowered(std::string text) -> std::string {
  std::ranges::transform(text, text.begin(),
                         [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return text;
}

} // namespace

class tree_item : public widget {
public:
  explicit tree_item(tree_view &owner) : owner_(owner) { set_minimum_target(true); }

  [[nodiscard]] auto index() const -> std::size_t { return index_; }
  [[nodiscard]] auto generation() const -> std::size_t { return generation_; }

  auto bind(std::size_t index, std::size_t generation) -> void {
    tree_row next = owner_.row(index);
    if (index == index_ && next == row_) {
      generation_ = generation;
      return;
    }
    index_ = index;
    generation_ = generation;
    text_.set_text(next.text);
    detail_.set_text(next.detail);
    row_ = std::move(next);
    invalidate_paint();
  }

  [[nodiscard]] auto accessibility_info() const -> accessibility::info override {
    accessibility::info details;
    details.kind = accessibility::role::tree_item;
    details.name = row_.text;
    details.description = row_.detail;
    details.level = row_.depth + 1;
    details.selected = owner_.selected_ == index_;
    details.position_in_set = row_.position_in_set;
    details.set_size = row_.set_size;
    details.actions = {accessibility::action::focus, accessibility::action::select,
                       accessibility::action::press, accessibility::action::scroll_into_view};
    if (row_.expandable) {
      details.expanded = row_.expanded;
      details.actions.insert(row_.expanded ? accessibility::action::collapse
                                           : accessibility::action::expand);
    }
    return details;
  }

  [[nodiscard]] auto focus_ring() const -> std::optional<focus_ring_shape> override {
    // Inside the row, because the scroll view clips whatever is drawn beyond it.
    rect const area{{}, bounds().extent};
    return focus_ring_shape{inset(area, insets::uniform(resolve(fixed(3)))), 0};
  }

protected:
  auto on_event(event_context &, event const &e) -> bool override {
    auto const *const p = std::get_if<pointer_event>(&e);
    if (p == nullptr || p->button != pointer_button::primary) {
      return false;
    }
    if (p->action == pointer_action::down) {
      bool const on_mark =
          row_.expandable && p->position.x >= mark_left() && p->position.x < text_left();
      owner_.press(index_, on_mark, p->click_count >= 2);
      return true;
    }
    return p->action == pointer_action::up;
  }

  auto on_accessibility_action(accessibility::action_request const &request) -> bool override {
    switch (request.kind) {
    case accessibility::action::select:
      owner_.user_select(index_, focus_reason::programmatic);
      return true;
    case accessibility::action::press:
      owner_.activate(index_);
      return true;
    case accessibility::action::expand:
    case accessibility::action::collapse:
      owner_.toggle(index_);
      return true;
    case accessibility::action::scroll_into_view:
      owner_.scroll_to_row(index_);
      return true;
    default:
      return false;
    }
  }

  auto on_interaction_changed(interaction_state const &previous) -> void override {
    widget::on_interaction_changed(previous);
    if (interaction().focused && !previous.focused) {
      owner_.scroll_to_row(index_);
    }
  }

  auto on_paint(render_context &context) const -> void override {
    if (tree() == nullptr) {
      return;
    }
    basalt::theme const &t = theme();
    rect const area{{}, bounds().extent};
    bool const hovered = interaction().hovered;
    if (owner_.selected_ == index_) {
      context.fill_rect(area, hovered ? t.colors.background_accent_subdued_hover
                                      : t.colors.background_accent_subdued);
    } else if (hovered) {
      context.fill_rect(area, t.colors.background_accent_transparent_hover);
    }
    float const mark = mark_extent();
    if (row_.expandable) {
      glyphs::draw_disclosure(context, {{mark_left(), (area.height() - mark) / 2}, {mark, mark}},
                              row_.expanded, t.colors.icon_default,
                              device_stroke_width(fixed(1.5f), scale()));
    }
    float const inset_x = resolve(t.sizes.inset_horizontal_list_item);
    float const left = text_left();
    float const right = area.width() - inset_x;
    float const detail_width = detail_.text().empty() ? 0 : detail_.measure(*this).width;
    float const text_right =
        detail_width > 0 ? right - detail_width - resolve(t.sizes.gap_horizontal_text_text) : right;
    {
      rect const text_area{{left, 0}, {std::max(0.0f, text_right - left), area.height()}};
      auto const clipped = context.clip(text_area);
      text_.draw_in(context, *this, text_area, t.colors.text_default);
    }
    if (detail_width > 0) {
      detail_.draw_in(context, *this, {{left, 0}, {std::max(0.0f, right - left), area.height()}},
                      t.colors.text_subdued, alignment::end);
    }
  }

private:
  [[nodiscard]] auto mark_extent() const -> float { return resolve(theme().sizes.dimension_icon); }
  [[nodiscard]] auto mark_left() const -> float {
    return resolve(theme().sizes.inset_horizontal_list_item) +
           static_cast<float>(row_.depth) * mark_extent();
  }
  [[nodiscard]] auto text_left() const -> float {
    return mark_left() + mark_extent() + resolve(theme().sizes.gap_horizontal_shape_text);
  }

  tree_view &owner_;
  std::size_t index_ = 0;
  std::size_t generation_ = 0;
  tree_row row_;
  text_line text_{"", text_role::body};
  text_line detail_{"", text_role::small};
};

// The scrolled content: as tall as every row, with widgets only for the rows that need them.
class tree_body : public widget {
public:
  explicit tree_body(tree_view &owner) : owner_(owner) {}

  [[nodiscard]] auto find(std::size_t index) const -> tree_item * {
    for (auto &c : children()) {
      auto &item = static_cast<tree_item &>(c);
      if (item.index() == index) {
        return &item;
      }
    }
    return nullptr;
  }

  // The row's widget, made now when the row has none, so that it can take focus before the next
  // layout.
  auto realize(std::size_t index) -> tree_item & {
    if (auto *const found = find(index)) {
      return *found;
    }
    auto &item = emplace_child<tree_item>(owner_);
    item.bind(index, generation_);
    place(item);
    return item;
  }

  // The data changed, so every realized row reads its row again.
  auto rows_changed() -> void {
    ++generation_;
    invalidate_layout();
  }

  auto update_focus_policies() -> void {
    std::size_t const anchor = owner_.anchor();
    for (auto &c : children()) {
      auto &item = static_cast<tree_item &>(c);
      // One row is the Tab stop; a click focuses any row.
      item.set_focus_policy(item.index() == anchor ? focus_policy::strong : focus_policy::click);
    }
  }

protected:
  [[nodiscard]] auto on_measure(constraints const &limits) const -> basalt::size override {
    return {limits.min.width, static_cast<float>(owner_.count_) * owner_.row_height()};
  }

  auto on_viewport_moved() -> void override { invalidate_layout(); }

  auto on_layout() -> void override {
    std::size_t const count = owner_.count_;
    float const height = owner_.row_height();
    rect const shown = owner_.scroller_->visible_content_rect();
    auto const row_at = [&](float y) {
      return std::min(count, static_cast<std::size_t>(std::max(0.0f, y / height)));
    };
    std::size_t const first = row_at(shown.top());
    std::size_t const last = std::min(count, row_at(std::ceil(shown.bottom() / height) * height));
    auto const wanted = [&](std::size_t index) {
      return index < count && ((index >= first && index < last) || index == owner_.anchor());
    };

    std::vector<tree_item *> spare;
    std::vector<bool> realized(last - first, false);
    bool anchor_realized = false;
    for (auto &c : children()) {
      auto &item = static_cast<tree_item &>(c);
      if (!wanted(item.index())) {
        spare.push_back(&item);
        continue;
      }
      if (item.index() >= first && item.index() < last) {
        realized[item.index() - first] = true;
      } else {
        anchor_realized = true;
      }
      if (item.generation() != generation_) {
        item.bind(item.index(), generation_);
      }
      place(item);
    }
    std::vector<std::size_t> missing;
    for (std::size_t i = first; i < last; ++i) {
      if (!realized[i - first]) {
        missing.push_back(i);
      }
    }
    std::size_t const anchor = owner_.anchor();
    if (!anchor_realized && anchor < count && (anchor < first || anchor >= last)) {
      missing.push_back(anchor);
    }
    for (std::size_t const index : missing) {
      tree_item *item = nullptr;
      if (!spare.empty()) {
        item = spare.back();
        spare.pop_back();
      } else {
        item = &emplace_child<tree_item>(owner_);
      }
      item->bind(index, generation_);
      place(*item);
    }
    for (tree_item *const item : spare) {
      (void)remove_child(*item);
    }
    update_focus_policies();
  }

private:
  auto place(tree_item &item) -> void {
    float const height = owner_.row_height();
    item.set_bounds({{0, static_cast<float>(item.index()) * height}, {bounds().width(), height}});
  }

  tree_view &owner_;
  std::size_t generation_ = 0;
};

tree_view::tree_view(std::string name) : name_(std::move(name)) {
  set_focus_scope(focus_scope::group);
  scroller_ = &emplace_child<scroll_view>(scroll_axes::vertical);
  scroller_->set_scrollbars(true);
  body_ = &scroller_->emplace_content<tree_body>(*this);
}

auto tree_view::set_rows(tree_rows rows) -> void {
  rows_ = std::move(rows);
  pending_.reset();
  rows_changed();
}

auto tree_view::rows_changed() -> void {
  count_ = rows_.count ? rows_.count() : 0;
  if (pending_ && selected_) {
    std::size_t &index = *selected_;
    if (index > pending_->index && index < pending_->subtree_end) {
      index = pending_->index;
    } else if (index >= pending_->subtree_end) {
      index = index + count_ - pending_->count;
    }
  }
  pending_.reset();
  if (selected_ && *selected_ >= count_) {
    selected_ = count_ == 0 ? std::nullopt : std::optional{count_ - 1};
  }
  body_->rows_changed();
  body_->invalidate_paint();
  place_selection(selected_, focus_reason::programmatic);
}

auto tree_view::row(std::size_t index) const -> tree_row {
  return rows_.row && index < count_ ? rows_.row(index) : tree_row{};
}

auto tree_view::depth_of(std::size_t index) const -> std::uint32_t {
  return rows_.depth ? rows_.depth(index) : row(index).depth;
}

auto tree_view::parent_of(std::size_t index) const -> std::optional<std::size_t> {
  std::uint32_t const depth = depth_of(index);
  for (std::size_t i = index; i-- > 0;) {
    if (depth_of(i) < depth) {
      return i;
    }
  }
  return std::nullopt;
}

auto tree_view::subtree_end(std::size_t index) const -> std::size_t {
  std::uint32_t const depth = depth_of(index);
  std::size_t end = index + 1;
  while (end < count_ && depth_of(end) > depth) {
    ++end;
  }
  return end;
}

auto tree_view::select(std::optional<std::size_t> index) -> void {
  if (index && *index >= count_) {
    index.reset();
  }
  place_selection(index, focus_reason::programmatic);
}

auto tree_view::place_selection(std::optional<std::size_t> index, focus_reason reason) -> void {
  if (selected_ != index) {
    body_->invalidate_paint();
  }
  selected_ = index;
  if (count_ == 0) {
    return;
  }
  tree_item &item = body_->realize(anchor());
  body_->update_focus_policies();
  if (interaction().focus_within && !item.has_focus()) {
    item.request_focus(reason);
  }
}

auto tree_view::scroll_to_row(std::size_t index) -> void {
  float const height = row_height();
  scroller_->scroll_into_view(
      {{0, static_cast<float>(index) * height}, {scroller_->viewport_size().width, height}});
}

auto tree_view::row_height() const -> float {
  auto const &t = theme();
  float const line = to_layout(text_style_for(t, text_role::body), scale()).line_height;
  return std::max(line + 2 * resolve(t.sizes.inset_vertical_list_item_text),
                  resolve(t.sizes.dimension_height_element_min));
}

auto tree_view::row_area(std::size_t index) const -> rect {
  float const height = row_height();
  return {{0, static_cast<float>(index) * height - scroller_->scroll_offset().y},
          {scroller_->viewport_size().width, height}};
}

auto tree_view::realized_row(std::size_t index) const -> widget const * {
  return index < count_ ? body_->find(index) : nullptr;
}

auto tree_view::realized_row_count() const -> std::size_t { return body_->child_count(); }

auto tree_view::rows_per_page() const -> std::size_t {
  return std::max<std::size_t>(
      1, static_cast<std::size_t>(scroller_->viewport_size().height / row_height()));
}

auto tree_view::move_to(std::size_t index, focus_reason reason) -> void {
  place_selection(index, reason);
  scroll_to_row(index);
  notify(on_select_, index);
}

auto tree_view::user_select(std::size_t index, focus_reason reason) -> void {
  place_selection(index, reason);
  notify(on_select_, index);
}

auto tree_view::activate(std::size_t index) -> void { notify(on_activate_, index); }

auto tree_view::press(std::size_t index, bool on_mark, bool twice) -> void {
  // Each handler may destroy the tree, so they run from copies and nothing is read after them.
  auto const on_select = on_select_;
  auto const on_activate = on_activate_;
  place_selection(index, focus_reason::pointer);
  if (on_mark) {
    toggle(index);
  }
  if (on_select) {
    on_select(index);
  }
  if (twice && !on_mark && on_activate) {
    on_activate(index);
  }
}

auto tree_view::toggle(std::size_t index) -> void {
  tree_row const current = row(index);
  if (!current.expandable) {
    return;
  }
  pending_ = pending_toggle{.index = index, .subtree_end = subtree_end(index), .count = count_};
  notify(on_expand_, index, !current.expanded);
}

auto tree_view::find_typed(std::string const &prefix, std::size_t from) const
    -> std::optional<std::size_t> {
  for (std::size_t step = 0; step < count_; ++step) {
    std::size_t const index = (from + step) % count_;
    if (lowered(row(index).text).starts_with(prefix)) {
      return index;
    }
  }
  return std::nullopt;
}

auto tree_view::type_ahead(text_input_event const &input) -> bool {
  if (count_ == 0 || input.text.empty() || (typed_.empty() && input.text == " ")) {
    return false;
  }
  if (input.time - typed_at_ > type_ahead_pause) {
    typed_.clear();
  }
  typed_at_ = input.time;
  bool const fresh = typed_.empty();
  typed_ += lowered(input.text);
  // A new search starts after the selected row, so repeating a letter steps through the rows
  // that start with it; a longer prefix may still match the selected row.
  std::size_t const from = !selected_ ? 0 : fresh ? *selected_ + 1 : *selected_;
  if (auto const found = find_typed(typed_, from); found && found != selected_) {
    move_to(*found, focus_reason::keyboard);
  }
  return true;
}

auto tree_view::on_event(event_context &, event const &e) -> bool {
  if (auto const *const input = std::get_if<text_input_event>(&e)) {
    return type_ahead(*input);
  }
  auto const *const k = std::get_if<key_event>(&e);
  if (k == nullptr || !k->pressed || k->modifiers != modifier::none || count_ == 0) {
    return false;
  }
  std::size_t const at = anchor();
  std::size_t const last = count_ - 1;
  std::size_t target = at;
  switch (k->code) {
  case key::down:
    target = selected_ ? std::min(at + 1, last) : at;
    break;
  case key::up:
    target = selected_ && at > 0 ? at - 1 : at;
    break;
  case key::page_down:
    target = std::min(at + rows_per_page(), last);
    break;
  case key::page_up:
    target = at - std::min(at, rows_per_page());
    break;
  case key::home:
    target = 0;
    break;
  case key::end:
    target = last;
    break;
  case key::right: {
    tree_row const current = row(at);
    if (current.expandable && !current.expanded) {
      toggle(at);
      return true;
    }
    if (current.expandable && at < last && depth_of(at + 1) > current.depth) {
      target = at + 1;
    }
    break;
  }
  case key::left: {
    tree_row const current = row(at);
    if (current.expandable && current.expanded) {
      toggle(at);
      return true;
    }
    target = parent_of(at).value_or(at);
    break;
  }
  case key::enter:
    activate(at);
    return true;
  default:
    return false;
  }
  if (selected_ != target) {
    move_to(target, focus_reason::keyboard);
  }
  return true;
}

auto tree_view::on_layout() -> void { scroller_->set_bounds({{}, bounds().extent}); }

auto tree_view::on_measure(constraints const &limits) const -> basalt::size {
  return {limits.min.width, static_cast<float>(count_) * row_height()};
}

auto tree_view::accessibility_info() const -> accessibility::info {
  accessibility::info details;
  details.kind = accessibility::role::tree;
  details.name = name_;
  details.orientation = accessibility::orientation::vertical;
  return details;
}

} // namespace basalt
