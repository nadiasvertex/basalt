#include "basalt/image.h"

#include <atomic>
#include <utility>

namespace basalt {

auto raster_image::create(std::uint32_t width, std::uint32_t height,
                          std::vector<std::uint8_t> pixels) -> raster_image_ptr {
  if (width == 0 || height == 0) {
    return nullptr;
  }
  auto const expected = std::size_t{width} * height * 4;
  if (pixels.size() != expected) {
    return nullptr;
  }
  static std::atomic<std::uint64_t> next_id{1};
  return std::shared_ptr<raster_image const>(
      new raster_image(next_id.fetch_add(1), width, height, std::move(pixels)));
}

} // namespace basalt
