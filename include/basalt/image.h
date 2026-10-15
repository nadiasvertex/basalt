#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace basalt {

// Decoded pixels for paint commands: 8-bit RGBA, straight (not premultiplied)
// alpha, rows top to bottom with no padding. Immutable once created and shared
// with paint commands, so repainting does not copy pixels. The id is unique per
// image so renderers can cache a device copy.
class raster_image {
public:
  // Returns null when the dimensions are zero or `pixels` is not exactly
  // width * height * 4 bytes.
  [[nodiscard]] static auto create(std::uint32_t width, std::uint32_t height,
                                   std::vector<std::uint8_t> pixels)
      -> std::shared_ptr<raster_image const>;

  [[nodiscard]] auto id() const -> std::uint64_t { return id_; }
  [[nodiscard]] auto width() const -> std::uint32_t { return width_; }
  [[nodiscard]] auto height() const -> std::uint32_t { return height_; }
  [[nodiscard]] auto pixels() const -> std::vector<std::uint8_t> const & { return pixels_; }

private:
  raster_image(std::uint64_t id, std::uint32_t width, std::uint32_t height,
               std::vector<std::uint8_t> pixels)
      : id_(id), width_(width), height_(height), pixels_(std::move(pixels)) {}

  std::uint64_t id_;
  std::uint32_t width_;
  std::uint32_t height_;
  std::vector<std::uint8_t> pixels_;
};

using raster_image_ptr = std::shared_ptr<raster_image const>;

} // namespace basalt
