// An image built in memory. The view scales it down to fit.

#include <cstdint>
#include <print>
#include <vector>

#include "example_host.h"

namespace {

auto gradient() -> basalt::raster_image_ptr {
  constexpr auto side = std::uint32_t{96};
  auto pixels = std::vector<std::uint8_t>{};
  for (auto y = std::uint32_t{0}; y < side; ++y) {
    for (auto x = std::uint32_t{0}; x < side; ++x) {
      pixels.insert(pixels.end(),
                    {static_cast<std::uint8_t>(x * 2), static_cast<std::uint8_t>(y * 2),
                     std::uint8_t{160}, std::uint8_t{255}});
    }
  }
  return basalt::raster_image::create(side, side, std::move(pixels));
}

} // namespace

auto main(int argc, char **argv) -> int {
  auto host = example::example_host{{112, 96}};

  auto &picture = host.content().emplace_child<basalt::image_view>();
  picture.set_image(gradient());
  picture.set_description("A color gradient");
  picture.set_maximum_height(basalt::fixed(64));

  host.frame();
  auto const area = picture.image_bounds();
  std::println("The image fills {} by {} layout pixels.", area.extent.width, area.extent.height);

  return host.finish(argc, argv);
}
