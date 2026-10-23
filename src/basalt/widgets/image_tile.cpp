#include "basalt/widgets/image_tile.h"

#include "basalt/render_context.h"
#include "basalt/widget_tree.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace basalt {

namespace {

constexpr length corner_margin = fixed(4);
constexpr length caption_gap = fixed(4);
constexpr length picture_radius = fixed(4);
constexpr length fallback_size = fixed(160);

} // namespace

auto image_tile::set_image(raster_image_ptr image) -> void {
  if (image == image_) {
    return;
  }
  image_ = std::move(image);
  invalidate_paint();
}

auto image_tile::set_caption(std::string caption) -> void {
  if (caption_.set_text(std::move(caption))) {
    invalidate_content();
  }
}

auto image_tile::set_selected(bool selected) -> void {
  if (selected != selected_) {
    selected_ = selected;
    invalidate_paint();
  }
}

auto image_tile::badge(tile_corner corner) const -> widget * {
  widget_id const id = badges_[static_cast<std::size_t>(corner)];
  return tree() != nullptr && id.valid() ? tree()->find(id) : nullptr;
}

auto image_tile::set_badge(tile_corner corner, std::unique_ptr<widget> badge) -> widget * {
  widget_id &slot = badges_[static_cast<std::size_t>(corner)];
  for (auto &c : children()) {
    if (c.id() == slot) {
      (void)remove_child(c);
      break;
    }
  }
  slot = {};
  if (badge == nullptr) {
    return nullptr;
  }
  widget &added = add_child(std::move(badge));
  slot = added.id();
  invalidate_layout();
  return &added;
}

auto image_tile::on_child_removed(widget &child) -> void {
  for (widget_id &slot : badges_) {
    if (slot == child.id()) {
      slot = {};
    }
  }
}

auto image_tile::caption_height() const -> float {
  return caption_.text().empty() ? 0 : caption_.measure(*this).height + resolve(caption_gap);
}

auto image_tile::picture_area() const -> rect {
  basalt::size const extent = bounds().extent;
  return {{}, {extent.width, std::max(0.0f, extent.height - caption_height())}};
}

auto image_tile::picture_bounds() const -> rect {
  rect const area = picture_area();
  if (image_ == nullptr || area.empty()) {
    return {};
  }
  float const aspect = static_cast<float>(image_->width()) / static_cast<float>(image_->height());
  float width = area.width();
  float height = width / aspect;
  if (height > area.height()) {
    height = area.height();
    width = height * aspect;
  }
  return {
      {area.origin.x + (area.width() - width) / 2, area.origin.y + (area.height() - height) / 2},
      {width, height}};
}

auto image_tile::on_measure(constraints const &limits) const -> basalt::size {
  float const fallback = resolve(fallback_size);
  return {std::isfinite(limits.max.width) ? limits.max.width : fallback,
          std::isfinite(limits.max.height) ? limits.max.height : fallback};
}

auto image_tile::on_layout() -> void {
  rect const area = picture_area();
  float const margin = resolve(corner_margin);
  for (std::size_t i = 0; i < corner_count; ++i) {
    widget *const w = badge(static_cast<tile_corner>(i));
    if (w == nullptr) {
      continue;
    }
    basalt::size const wanted = w->measure({});
    bool const right = i == static_cast<std::size_t>(tile_corner::top_right) ||
                       i == static_cast<std::size_t>(tile_corner::bottom_right);
    bool const bottom = i == static_cast<std::size_t>(tile_corner::bottom_left) ||
                        i == static_cast<std::size_t>(tile_corner::bottom_right);
    float const x = right ? area.width() - margin - wanted.width : margin;
    float const y = bottom ? area.height() - margin - wanted.height : margin;
    w->set_bounds({{x, y}, wanted});
  }
}

auto image_tile::on_paint(render_context &context) const -> void {
  if (tree() == nullptr) {
    return;
  }
  basalt::theme const &t = theme();
  rect const area = picture_area();
  float const radius = resolve(picture_radius);
  context.fill_rounded_rect(area, radius, t.colors.background_tertiary);
  if (image_ != nullptr) {
    context.draw_image(picture_bounds(), image_);
  }
  if (selected_) {
    float const width = device_stroke_width(fixed(2), scale());
    context.stroke_rounded_rect(inset(area, insets::uniform(width / 2)), radius,
                                t.colors.border_accent, width);
  }
  if (!caption_.text().empty()) {
    float const top = area.height() + resolve(caption_gap);
    caption_.draw_in(context, *this, {{0, top}, {bounds().width(), bounds().height() - top}},
                     t.colors.text_default, alignment::start, alignment::start);
  }
}

auto image_tile::accessibility_info() const -> accessibility::info {
  accessibility::info details;
  details.kind = accessibility::role::image;
  details.name = description_.empty() ? caption_.text() : description_;
  details.selected = selected_;
  return details;
}

} // namespace basalt
