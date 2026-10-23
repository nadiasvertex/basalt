#pragma once

#include "basalt/image.h"
#include "basalt/widget.h"
#include "basalt/widgets/text_line.h"

#include <cstdint>
#include <memory>
#include <string>

namespace basalt {

enum class tile_corner : std::uint8_t { top_left, top_right, bottom_left, bottom_right };

// A picture with an optional caption, made for the cells of a grid. The picture
// is scaled to fit the area above the caption, up or down, and keeps its aspect
// ratio. Without a picture the area shows a plain placeholder, so a tile can
// appear before its picture has loaded.
//
// Each corner of the picture area has a slot for a badge. A badge is any widget.
// The tile measures it at its natural size and places it in the corner.
//
// The tile takes the size its parent gives it. A grid cell sets the size.
class image_tile : public widget {
public:
  image_tile() = default;

  [[nodiscard]] auto image() const -> raster_image_ptr const & { return image_; }
  auto set_image(raster_image_ptr image) -> void;

  [[nodiscard]] auto caption() const -> std::string const & { return caption_.text(); }
  // An empty caption removes the caption row.
  auto set_caption(std::string caption) -> void;

  // The text assistive technology reads. It falls back to the caption.
  [[nodiscard]] auto description() const -> std::string const & { return description_; }
  auto set_description(std::string description) -> void { description_ = std::move(description); }

  // A selected tile shows an accent border.
  [[nodiscard]] auto selected() const -> bool { return selected_; }
  auto set_selected(bool selected) -> void;

  // Replaces the badge in `corner`, destroying the old one. A null badge
  // removes it. Returns the badge, or null when none was given.
  auto set_badge(tile_corner corner, std::unique_ptr<widget> badge) -> widget *;
  // Null when the corner has no badge.
  [[nodiscard]] auto badge(tile_corner corner) const -> widget *;

  // The picture area, and the rectangle the picture takes inside it, in this
  // widget's coordinates. The second is empty without a picture.
  [[nodiscard]] auto picture_area() const -> rect;
  [[nodiscard]] auto picture_bounds() const -> rect;

  [[nodiscard]] auto accessibility_info() const -> accessibility::info override;

protected:
  auto on_paint(render_context &context) const -> void override;
  auto on_layout() -> void override;
  [[nodiscard]] auto on_measure(constraints const &limits) const -> basalt::size override;
  auto on_child_removed(widget &child) -> void override;

private:
  static constexpr std::size_t corner_count = 4;

  [[nodiscard]] auto caption_height() const -> float;

  raster_image_ptr image_;
  text_line caption_{{}, text_role::small};
  std::string description_;
  bool selected_ = false;
  widget_id badges_[corner_count];
};

} // namespace basalt
