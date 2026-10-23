#include "basalt/widgets/collection_view.h"

#include "basalt/event_router.h"
#include "basalt/widget_tree.h"

#include <algorithm>
#include <cmath>
#include <ranges>
#include <unordered_map>
#include <utility>
#include <variant>

#include "notify.h"

namespace basalt {

// The content of the grid's scroll view. It owns the realized cells and
// headers and keeps them in step with what the scroll view shows.
class collection_body : public widget {
public:
  explicit collection_body(collection_view &owner) : owner_(owner) {}

  // The data changed, so every cell in view goes through the factory again.
  auto reload() -> void {
    refresh_ = true;
    invalidate_layout();
  }

  [[nodiscard]] auto cell(std::size_t index) const -> widget * {
    auto const found = cells_.find(index);
    return found == cells_.end() || tree() == nullptr ? nullptr : tree()->find(found->second);
  }

protected:
  [[nodiscard]] auto on_measure(constraints const &limits) const -> basalt::size override {
    float const width = std::isfinite(limits.max.width) ? limits.max.width : limits.min.width;
    return {width, owner_.layout_for(width).total_height()};
  }

  auto on_viewport_moved() -> void override { invalidate_layout(); }

  auto on_layout() -> void override {
    grid_layout const &geometry = owner_.layout_for(bounds().width());
    rect const shown = owner_.scroller_->visible_content_rect();
    index_range const strict = geometry.visible_items(shown.top(), shown.bottom());
    index_range const wanted =
        geometry.visible_items(shown.top(), shown.bottom(), owner_.margin_rows_);

    settle_cells(geometry, wanted);
    settle_headers(geometry, shown);
    refresh_ = false;

    owner_.realized_ = wanted;
    owner_.report_visible(strict);
  }

  auto on_event(event_context &, event const &e) -> bool override {
    auto const *const p = std::get_if<pointer_event>(&e);
    if (p == nullptr || p->button != pointer_button::primary) {
      return false;
    }
    switch (p->action) {
    case pointer_action::down:
      pressed_ = index_at(p->position);
      return pressed_.has_value();
    case pointer_action::up: {
      auto const pressed = std::exchange(pressed_, std::nullopt);
      if (pressed && index_at(p->position) == pressed) {
        owner_.move_current(pressed);
        owner_.activate(*pressed);
      }
      return pressed.has_value();
    }
    case pointer_action::cancel:
      pressed_.reset();
      return false;
    default:
      return false;
    }
  }

private:
  [[nodiscard]] auto index_at(point where) const -> std::optional<std::size_t> {
    for (auto const &[index, id] : cells_) {
      widget const *const w = tree() == nullptr ? nullptr : tree()->find(id);
      if (w != nullptr && w->bounds().contains(where)) {
        return index;
      }
    }
    return std::nullopt;
  }

  auto take(std::unordered_map<std::size_t, widget_id>::iterator found) -> std::unique_ptr<widget> {
    widget *const w = tree()->find(found->second);
    cells_.erase(found);
    return w == nullptr ? nullptr : remove_child(*w);
  }

  auto pool_or_destroy(std::unique_ptr<widget> spare, std::size_t limit) -> void {
    if (spare != nullptr && pool_.size() < limit) {
      pool_.push_back(std::move(spare));
    }
  }

  auto settle_cells(grid_layout const &geometry, index_range wanted) -> void {
    std::size_t const pool_limit = std::max<std::size_t>(32, wanted.size());
    // Cells that fell out of the margin go to the pool before new ones are
    // made, so they can be reused in this pass.
    std::vector<std::size_t> gone;
    for (auto const &entry : cells_) {
      if (!wanted.contains(entry.first)) {
        gone.push_back(entry.first);
      }
    }
    for (std::size_t const index : gone) {
      pool_or_destroy(take(cells_.find(index)), pool_limit);
    }

    for (std::size_t const index : std::views::iota(wanted.first, wanted.last)) {
      auto const found = cells_.find(index);
      std::unique_ptr<widget> recycled;
      if (found != cells_.end()) {
        if (!refresh_) {
          place(*cell(index), geometry.item_rect(index));
          continue;
        }
        recycled = take(found);
      } else if (!pool_.empty()) {
        recycled = std::move(pool_.back());
        pool_.pop_back();
      }
      if (!owner_.cell_factory_) {
        pool_or_destroy(std::move(recycled), pool_limit);
        continue;
      }
      std::unique_ptr<widget> made = owner_.cell_factory_(index, std::move(recycled));
      if (made == nullptr) {
        continue;
      }
      widget &added = add_child(std::move(made));
      cells_.emplace(index, added.id());
      place(added, geometry.item_rect(index));
    }
  }

  auto settle_headers(grid_layout const &geometry, rect shown) -> void {
    index_range const wanted = geometry.visible_headers(shown.top(), shown.bottom());
    std::vector<std::size_t> gone;
    for (auto const &entry : headers_) {
      if (!wanted.contains(entry.first) || refresh_) {
        gone.push_back(entry.first);
      }
    }
    for (std::size_t const section : gone) {
      auto const found = headers_.find(section);
      if (widget *const w = tree()->find(found->second)) {
        (void)remove_child(*w);
      }
      headers_.erase(found);
    }
    if (!owner_.header_factory_) {
      return;
    }
    for (std::size_t const section : std::views::iota(wanted.first, wanted.last)) {
      auto const found = headers_.find(section);
      if (found != headers_.end()) {
        place(*tree()->find(found->second), geometry.header_rect(section));
        continue;
      }
      std::unique_ptr<widget> made = owner_.header_factory_(section);
      if (made == nullptr) {
        continue;
      }
      widget &added = add_child(std::move(made));
      headers_.emplace(section, added.id());
      place(added, geometry.header_rect(section));
    }
  }

  static auto place(widget &w, rect area) -> void { w.set_bounds(area); }

  collection_view &owner_;
  bool refresh_ = false;
  std::unordered_map<std::size_t, widget_id> cells_;
  std::unordered_map<std::size_t, widget_id> headers_;
  // Cells that left the view, kept for the factory to reuse.
  std::vector<std::unique_ptr<widget>> pool_;
  std::optional<std::size_t> pressed_;
};

collection_view::collection_view(std::string name) : name_(std::move(name)) {
  set_focus_policy(focus_policy::strong);
  scroller_ = &emplace_child<scroll_view>(scroll_axes::vertical);
  scroller_->set_scrollbars(true);
  body_ = &scroller_->emplace_content<collection_body>(*this);
}

collection_view::~collection_view() = default;

auto collection_view::geometry_changed() -> void {
  layout_stale_ = true;
  item_count_ = 0;
  for (grid_section const &s : sections_) {
    item_count_ += s.count;
  }
  if (current_ && *current_ >= item_count_) {
    current_ = item_count_ == 0 ? std::nullopt : std::optional{item_count_ - 1};
  }
  body_->reload();
  invalidate_layout();
}

auto collection_view::set_item_count(std::size_t count) -> void { set_sections({{count, false}}); }

auto collection_view::set_sections(std::vector<grid_section> sections) -> void {
  sections_ = std::move(sections);
  geometry_changed();
}

auto collection_view::set_cell_size(length width, length height) -> void {
  if (width == cell_width_ && height == cell_height_) {
    return;
  }
  cell_width_ = width;
  cell_height_ = height;
  geometry_changed();
}

auto collection_view::set_gap(length gap) -> void {
  if (gap != gap_) {
    gap_ = gap;
    geometry_changed();
  }
}

auto collection_view::set_header_height(length height) -> void {
  if (height != header_height_) {
    header_height_ = height;
    geometry_changed();
  }
}

auto collection_view::set_margin_rows(std::size_t rows) -> void {
  if (rows != margin_rows_) {
    margin_rows_ = rows;
    body_->invalidate_layout();
  }
}

auto collection_view::set_cell_factory(cell_factory factory) -> void {
  cell_factory_ = std::move(factory);
  body_->reload();
}

auto collection_view::set_header_factory(header_factory factory) -> void {
  header_factory_ = std::move(factory);
  body_->reload();
}

auto collection_view::reload() -> void { body_->reload(); }

auto collection_view::cell(std::size_t index) const -> widget * { return body_->cell(index); }

auto collection_view::on_environment_changed() -> void {
  layout_stale_ = true;
  body_->reload();
}

auto collection_view::layout_for(float width) const -> grid_layout const & {
  if (layout_stale_ || width != layout_width_) {
    layout_ = grid_layout(width, {resolve(cell_width_), resolve(cell_height_)}, resolve(gap_),
                          resolve(header_height_), sections_);
    layout_width_ = width;
    layout_stale_ = false;
  }
  return layout_;
}

auto collection_view::layout() const -> grid_layout const & { return layout_for(bounds().width()); }

auto collection_view::on_measure(constraints const &limits) const -> basalt::size {
  float const width = std::isfinite(limits.max.width) ? limits.max.width : resolve(cell_width_);
  float const height =
      std::isfinite(limits.max.height) ? limits.max.height : 3 * resolve(cell_height_);
  return {width, height};
}

auto collection_view::on_layout() -> void {
  scroller_->set_bounds({{}, bounds().extent});
  if (pending_reveal_ && *pending_reveal_ < layout().item_count()) {
    scroller_->scroll_into_view(layout().item_rect(*pending_reveal_));
  }
  pending_reveal_.reset();
}

auto collection_view::report_visible(index_range visible) -> void {
  if (visible == visible_) {
    return;
  }
  visible_ = visible;
  notify(on_visible_change_, visible);
}

auto collection_view::scroll_to_item(std::size_t index) -> void {
  if (index >= item_count_) {
    return;
  }
  if (bounds().width() > 0) {
    scroller_->scroll_into_view(layout().item_rect(index));
  } else {
    pending_reveal_ = index;
    invalidate_layout();
  }
}

auto collection_view::scroll_offset() const -> float { return scroller_->scroll_offset().y; }

auto collection_view::set_scroll_offset(float offset) -> void {
  scroller_->set_scroll_offset({0, offset});
}

auto collection_view::set_current(std::optional<std::size_t> index) -> void {
  if (index && *index >= item_count_) {
    index = item_count_ == 0 ? std::nullopt : std::optional{item_count_ - 1};
  }
  current_ = index;
  if (index) {
    scroll_to_item(*index);
  }
  invalidate_paint();
}

auto collection_view::move_current(std::optional<std::size_t> target) -> bool {
  if (!target || *target >= item_count_ || target == current_) {
    return false;
  }
  set_current(target);
  notify(on_current_change_, *target);
  return true;
}

auto collection_view::activate(std::size_t index) -> void { notify(on_activate_, index); }

auto collection_view::handle_key(key code) -> bool {
  if (item_count_ == 0) {
    return false;
  }
  grid_layout const &geometry = layout();
  if (code == key::enter || code == key::space) {
    if (!current_) {
      return false;
    }
    activate(*current_);
    return true;
  }
  bool const moves = code == key::left || code == key::right || code == key::up ||
                     code == key::down || code == key::home || code == key::end ||
                     code == key::page_up || code == key::page_down;
  if (!moves) {
    return false;
  }
  if (!current_ && code != key::home && code != key::end) {
    // The first movement picks the first item in view.
    move_current(std::min(visible_.first, item_count_ - 1));
    return true;
  }
  std::size_t const here = current_.value_or(0);
  switch (code) {
  case key::left:
    move_current(here > 0 ? std::optional{here - 1} : std::nullopt);
    return true;
  case key::right:
    move_current(here + 1 < item_count_ ? std::optional{here + 1} : std::nullopt);
    return true;
  case key::up:
    move_current(geometry.item_above(here));
    return true;
  case key::down:
    move_current(geometry.item_below(here));
    return true;
  case key::home:
    move_current(0);
    return true;
  case key::end:
    move_current(item_count_ - 1);
    return true;
  case key::page_up:
  case key::page_down: {
    // One page is the rows that fit in the view, and at least one.
    float const pitch = resolve(cell_height_) + resolve(gap_);
    auto const rows = std::max<std::size_t>(
        1, pitch > 0 ? static_cast<std::size_t>(bounds().height() / pitch) : 1);
    std::optional<std::size_t> target = here;
    for (std::size_t n = 0; n < rows && target; ++n) {
      auto const next =
          code == key::page_up ? geometry.item_above(*target) : geometry.item_below(*target);
      if (!next) {
        break;
      }
      target = next;
    }
    move_current(target);
    return true;
  }
  default:
    return false;
  }
}

auto collection_view::on_event(event_context &, event const &e) -> bool {
  auto const *const k = std::get_if<key_event>(&e);
  if (k == nullptr || !k->pressed || k->modifiers != modifier::none) {
    return false;
  }
  return handle_key(k->code);
}

auto collection_view::focus_ring() const -> std::optional<focus_ring_shape> {
  auto ring = widget::focus_ring();
  if (!ring || !current_ || *current_ >= layout().item_count()) {
    return ring;
  }
  rect const cell_area = layout().item_rect(*current_);
  ring->area = translated(cell_area, {0, -scroll_offset()});
  return ring;
}

auto collection_view::accessibility_info() const -> accessibility::info {
  accessibility::info details;
  details.kind = accessibility::role::list;
  details.name = name_;
  details.actions.insert(accessibility::action::focus);
  return details;
}

} // namespace basalt
