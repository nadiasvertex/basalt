#include <catch2/catch_test_macros.hpp>

#include "basalt/widgets/box.h"
#include "basalt/widgets/image_view.h"

#include <variant>
#include <vector>

#include "ui_fixture.h"

using namespace basalt;
using namespace basalt::testing;

namespace {

auto make_image(std::uint32_t width, std::uint32_t height) -> raster_image_ptr {
  return raster_image::create(width, height, std::vector<std::uint8_t>(width * height * 4, 255));
}

} // namespace

TEST_CASE("raster images validate their pixels", "[image]") {
  CHECK(make_image(2, 3) != nullptr);
  CHECK(raster_image::create(0, 3, {}) == nullptr);
  CHECK(raster_image::create(2, 2, std::vector<std::uint8_t>(15)) == nullptr);
  CHECK(make_image(1, 1)->id() != make_image(1, 1)->id());
}

TEST_CASE("an image view measures the natural size", "[image_view]") {
  ui f({400, 300});
  auto &root = f.tree.emplace_root<widget>();
  auto &view = root.emplace_child<image_view>();
  f.layout();
  CHECK(view.measure({}) == basalt::size{});
  view.set_image(make_image(40, 20));
  f.layout();
  CHECK(view.measure({}) == basalt::size{40, 20});
}

TEST_CASE("an image view shrinks to fit but never enlarges", "[image_view]") {
  ui f({400, 300});
  auto &root = f.tree.emplace_root<widget>();
  auto &view = root.emplace_child<image_view>();
  view.set_image(make_image(200, 100));
  f.layout();
  CHECK(view.measure(constraints::loose({100, 100})) == basalt::size{100, 50});
  CHECK(view.measure(constraints::loose({1000, 1000})) == basalt::size{200, 100});
}

TEST_CASE("an image view paints its image once", "[image_view][paint]") {
  ui f({400, 300});
  auto &root = f.tree.emplace_root<box>(axis::vertical);
  auto &view = root.emplace_child<image_view>();
  auto const image = make_image(10, 10);
  view.set_image(image);
  f.layout();
  auto drawn = 0;
  for (auto const &command : f.commands()) {
    if (auto const *draw = std::get_if<draw_image_command>(&command)) {
      ++drawn;
      CHECK(draw->image == image);
    }
  }
  CHECK(drawn == 1);
  view.set_image(nullptr);
  f.layout();
  for (auto const &command : f.commands()) {
    CHECK_FALSE(std::holds_alternative<draw_image_command>(command));
  }
}

TEST_CASE("an image view is an image to assistive technology", "[image_view][accessibility]") {
  image_view view;
  view.set_description("Logo");
  auto const details = view.accessibility_info();
  CHECK(details.kind == accessibility::role::image);
  CHECK(details.name == "Logo");
}
