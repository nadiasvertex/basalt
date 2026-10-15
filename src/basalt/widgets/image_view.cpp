#include "basalt/widgets/image_view.h"

#include "basalt/render_context.h"

#include <algorithm>
#include <utility>

namespace basalt {

auto image_view::set_image(raster_image_ptr image) -> void {
  if (image == image_) {
    return;
  }
  image_ = std::move(image);
  invalidate_layout();
  invalidate_paint();
}

auto image_view::set_description(std::string description) -> void {
  description_ = std::move(description);
}

auto image_view::set_maximum_height(std::optional<length> height) -> void {
  maximum_height_ = height;
  invalidate_layout();
}

auto image_view::fitted(basalt::size available) const -> basalt::size {
  if (image_ == nullptr) {
    return {};
  }
  // Natural size maps one image pixel to one device pixel.
  auto const display = std::max(scale().display, 0.01F);
  auto const width = static_cast<float>(image_->width()) / display;
  auto const height = static_cast<float>(image_->height()) / display;
  auto factor = 1.0F;
  if (available.width < width) {
    factor = std::min(factor, available.width / width);
  }
  if (available.height < height) {
    factor = std::min(factor, available.height / height);
  }
  return {width * factor, height * factor};
}

auto image_view::image_bounds() const -> rect {
  auto const extent = fitted(bounds().extent);
  auto const outer = bounds().extent;
  return {{(outer.width - extent.width) / 2, (outer.height - extent.height) / 2}, extent};
}

auto image_view::accessibility_info() const -> accessibility::info {
  accessibility::info details;
  details.kind = accessibility::role::image;
  details.name = description_;
  return details;
}

auto image_view::on_measure(constraints const &limits) const -> basalt::size {
  auto available = limits.max;
  if (maximum_height_) {
    available.height = std::min(available.height, resolve(*maximum_height_));
  }
  return fitted(available);
}

auto image_view::on_paint(render_context &context) const -> void {
  if (image_ == nullptr) {
    return;
  }
  context.draw_image(image_bounds(), image_);
}

} // namespace basalt
