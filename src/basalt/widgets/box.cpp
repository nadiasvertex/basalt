#include "basalt/widgets/box.h"

#include "basalt/render_context.h"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace basalt {

auto box::set_direction(axis direction) -> void {
  if (direction != direction_) {
    direction_ = direction;
    invalidate_layout();
  }
}

auto box::set_gap(length gap) -> void {
  if (gap != gap_) {
    gap_ = gap;
    invalidate_layout();
  }
}

auto box::set_padding(length_insets padding) -> void {
  if (padding != padding_) {
    padding_ = padding;
    invalidate_layout();
  }
}

auto box::set_main_alignment(alignment a) -> void {
  if (a != main_alignment_) {
    main_alignment_ = a;
    invalidate_layout();
  }
}

auto box::set_cross_alignment(alignment a) -> void {
  if (a != cross_alignment_) {
    cross_alignment_ = a;
    invalidate_layout();
  }
}

auto box::set_surface(std::optional<box_surface> surface) -> void {
  if (surface != surface_) {
    surface_ = surface;
    invalidate_paint();
  }
}

auto box::item(widget const &child) const -> box_item {
  auto const it = items_.find(child.id());
  return it == items_.end() ? box_item{} : it->second;
}

auto box::set_item(widget const &child, box_item item) -> bool {
  if (child.parent() != this) {
    return false;
  }
  auto &slot = items_[child.id()];
  if (slot != item) {
    slot = std::move(item);
    invalidate_layout();
  }
  return true;
}

auto box::on_child_removed(widget &child) -> void { items_.erase(child.id()); }

auto box::on_paint(render_context &context) const -> void {
  if (!surface_ || tree() == nullptr) {
    return;
  }
  basalt::theme const &t = theme();
  rect const area{{}, bounds().extent};
  float const radius = resolve(surface_->radius);
  float const width = device_stroke_width(fixed(1), scale());
  context.fill_rounded_rect(area, radius, t.colors.*surface_->fill);
  context.stroke_rounded_rect(inset(area, insets::uniform(width / 2)), radius,
                              t.colors.*surface_->border, width);
}

auto box::child_limits(float cross_max) const -> constraints {
  return {{}, make_size(direction_, unbounded, cross_max)};
}

auto box::preferred_main(box_item const &item, basalt::size measured) const -> float {
  return item.basis ? resolve(*item.basis) : main_extent(measured, direction_);
}

auto box::on_measure(constraints const &limits) const -> basalt::size {
  insets const pad = resolve(padding_);
  constraints const inner = limits.deflate(pad);
  constraints const each = child_limits(cross_extent(inner.max, direction_));
  float main = 0;
  float cross = 0;
  std::size_t count = 0;
  for (auto const &c : children()) {
    if (!c.visible()) {
      continue;
    }
    box_item const it = item(c);
    basalt::size const measured = c.measure(each);
    float const lo = resolve(it.min);
    float const hi = it.max ? std::max(lo, resolve(*it.max)) : unbounded;
    main += std::clamp(preferred_main(it, measured), lo, hi);
    cross = std::max(cross, cross_extent(measured, direction_));
    ++count;
  }
  if (count > 1) {
    main += resolve(gap_) * static_cast<float>(count - 1);
  }
  return inflate(make_size(direction_, main, cross), pad);
}

auto box::on_layout() -> void {
  rect const content = inset({{}, bounds().extent}, resolve(padding_));
  float const content_main = main_extent(content.extent, direction_);
  float const content_cross = cross_extent(content.extent, direction_);
  float const gap = resolve(gap_);
  constraints const each = child_limits(content_cross);

  struct slot {
    widget *child;
    alignment align;
    basalt::size measured;
  };
  std::vector<slot> slots;
  std::vector<linear_item> lines;
  for (auto &c : children()) {
    if (!c.visible()) {
      continue;
    }
    box_item const it = item(c);
    alignment const align = it.align.value_or(cross_alignment_);
    // A child with a basis that is stretched across the box has no use for its natural size,
    // and measuring it can be expensive.
    basalt::size const measured =
        it.basis && align == alignment::stretch ? basalt::size{} : c.measure(each);
    float const lo = resolve(it.min);
    lines.push_back({
        .basis = preferred_main(it, measured),
        .min = lo,
        .max = it.max ? std::max(lo, resolve(*it.max)) : unbounded,
        .grow = it.grow,
        .shrink = it.shrink,
    });
    slots.push_back({&c, align, measured});
  }
  if (slots.empty()) {
    return;
  }

  float const gaps = gap * static_cast<float>(slots.size() - 1);
  std::vector<float> const sizes = distribute(lines, content_main - gaps);
  float used = gaps;
  for (auto const s : sizes) {
    used += s;
  }

  scale_factors const factors = scale();
  float position = main_coordinate(content.origin, direction_) +
                   align_offset(main_alignment_, std::max(0.0f, content_main - used));
  float const cross_start = main_coordinate(content.origin, cross_axis(direction_));
  for (std::size_t i = 0; i < slots.size(); ++i) {
    auto const &[child, align, measured] = slots[i];
    float const cross = align == alignment::stretch
                            ? content_cross
                            : std::min(cross_extent(measured, direction_), content_cross);
    point const origin =
        make_point(direction_, position, cross_start + align_offset(align, content_cross - cross));
    child->set_bounds(
        snap_to_device(rect{origin, make_size(direction_, sizes[i], cross)}, factors));
    position += sizes[i] + gap;
  }
}

} // namespace basalt
