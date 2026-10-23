// A tile with a picture, a caption and a badge in one corner.

#include <cstdint>
#include <memory>
#include <print>
#include <vector>

#include "example_host.h"

auto main(int argc, char **argv) -> int {
  auto host = example::example_host{{200, 180}};

  constexpr std::uint32_t width = 64;
  constexpr std::uint32_t height = 40;
  std::vector<std::uint8_t> pixels;
  pixels.reserve(width * height * 4);
  for (std::uint32_t y = 0; y < height; ++y) {
    for (std::uint32_t x = 0; x < width; ++x) {
      pixels.insert(pixels.end(), {static_cast<std::uint8_t>(x * 4), static_cast<std::uint8_t>(y * 6),
                                   180, 255});
    }
  }

  auto &tile = host.content().emplace_child<basalt::image_tile>();
  host.content().set_cross_alignment(basalt::alignment::stretch);
  tile.set_image(basalt::raster_image::create(width, height, std::move(pixels)));
  tile.set_caption("IMG_0042.jpg");
  tile.set_description("A gradient from blue to green");
  tile.set_badge(basalt::tile_corner::top_right, std::make_unique<basalt::chip>("Keep"));
  tile.set_selected(true);

  host.frame();
  std::println("The picture takes {:.0f} by {:.0f} pixels.", tile.picture_bounds().width(),
               tile.picture_bounds().height());

  return host.finish(argc, argv);
}
