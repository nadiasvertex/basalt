#pragma once

#include "basalt/image.h"
#include "basalt/units.h"
#include "basalt/widget.h"

#include <optional>
#include <string>

namespace basalt {

// Shows one image, centered in its bounds. The image is scaled down to fit
// while keeping its aspect ratio and is never enlarged past its natural size.
// Its natural size is one device pixel per image pixel. It paints nothing when it has
// no image.
class image_view : public widget {
public:
  image_view() = default;

  [[nodiscard]] auto image() const -> raster_image_ptr const & { return image_; }
  auto set_image(raster_image_ptr image) -> void;

  // Limits the height the view asks for; the image is scaled down to fit. Unset
  // by default, so the view takes whatever its parent allows.
  [[nodiscard]] auto maximum_height() const -> std::optional<length> { return maximum_height_; }
  auto set_maximum_height(std::optional<length> height) -> void;

  // The text assistive technology reads in place of the picture.
  [[nodiscard]] auto description() const -> std::string const & { return description_; }
  auto set_description(std::string description) -> void;

  // The rectangle the image occupies inside the widget's bounds, in the
  // widget's own coordinates. Empty when there is no image.
  [[nodiscard]] auto image_bounds() const -> rect;

  [[nodiscard]] auto accessibility_info() const -> accessibility::info override;

protected:
  auto on_paint(render_context &context) const -> void override;
  [[nodiscard]] auto on_measure(constraints const &limits) const -> basalt::size override;

private:
  // The size at which the image fits within `available`.
  [[nodiscard]] auto fitted(basalt::size available) const -> basalt::size;

  raster_image_ptr image_;
  std::string description_;
  std::optional<length> maximum_height_;
};

} // namespace basalt
