#include <catch2/catch_test_macros.hpp>

#include "basalt/widgets/image_tile.h"

#include <memory>
#include <vector>

#include "leaf.h"
#include "text/font_support.h"
#include "ui_fixture.h"

using namespace basalt;
using namespace basalt::testing;
namespace a11y = basalt::accessibility;

namespace {

auto picture(std::uint32_t width, std::uint32_t height) -> raster_image_ptr {
  return raster_image::create(width, height, std::vector<std::uint8_t>(width * height * 4, 200));
}

struct fixture {
  fixture() {
    tile = &f.tree.emplace_root<image_tile>();
    f.tree.set_viewport({200, 200});
    f.layout();
  }

  ui f;
  image_tile *tile = nullptr;
};

} // namespace

TEST_CASE("a picture is scaled up or down to fit the tile and centered", "[image_tile]") {
  fixture f;
  f.tile->set_image(picture(40, 20));
  f.f.layout();
  REQUIRE(f.tile->picture_bounds() == rect{{0, 50}, {200, 100}});
  f.tile->set_image(picture(20, 40));
  REQUIRE(f.tile->picture_bounds() == rect{{50, 0}, {100, 200}});
  f.tile->set_image(picture(10, 10));
  REQUIRE(f.tile->picture_bounds() == rect{{0, 0}, {200, 200}});
}

TEST_CASE("a tile without a picture has no picture bounds", "[image_tile]") {
  fixture f;
  REQUIRE(f.tile->picture_bounds().empty());
  REQUIRE(f.tile->picture_area() == rect{{0, 0}, {200, 200}});
}

TEST_CASE("a caption takes room from the bottom of the picture area", "[image_tile]") {
  text::text_shaper shaper{basalt::test::bundled_fonts()};
  ui f;
  f.tree.set_text_shaper(&shaper);
  auto &tile = f.tree.emplace_root<image_tile>();
  f.tree.set_viewport({200, 200});
  f.layout();
  REQUIRE(tile.picture_area().height() == 200);
  tile.set_caption("IMG_0042.jpg");
  f.layout();
  REQUIRE(tile.picture_area().height() < 200);
  REQUIRE(tile.picture_area().width() == 200);
  tile.set_caption("");
  REQUIRE(tile.picture_area().height() == 200);
}

TEST_CASE("badges sit in their corners of the picture area", "[image_tile]") {
  fixture f;
  auto *const top_left =
      f.tile->set_badge(tile_corner::top_left, std::make_unique<leaf>(size{20, 10}));
  auto *const bottom_right =
      f.tile->set_badge(tile_corner::bottom_right, std::make_unique<leaf>(size{30, 12}));
  f.f.layout();
  REQUIRE(top_left->bounds() == rect{{4, 4}, {20, 10}});
  REQUIRE(bottom_right->bounds() == rect{{200 - 4 - 30, 200 - 4 - 12}, {30, 12}});
  REQUIRE(f.tile->badge(tile_corner::top_right) == nullptr);
  REQUIRE(f.tile->badge(tile_corner::top_left) == top_left);
}

TEST_CASE("setting a badge replaces the old one and null removes it", "[image_tile]") {
  fixture f;
  f.tile->set_badge(tile_corner::top_right, std::make_unique<leaf>(size{10, 10}));
  REQUIRE(f.tile->child_count() == 1);
  auto *const next =
      f.tile->set_badge(tile_corner::top_right, std::make_unique<leaf>(size{12, 12}));
  REQUIRE(f.tile->child_count() == 1);
  REQUIRE(f.tile->badge(tile_corner::top_right) == next);
  REQUIRE(f.tile->set_badge(tile_corner::top_right, nullptr) == nullptr);
  REQUIRE(f.tile->child_count() == 0);
  REQUIRE(f.tile->badge(tile_corner::top_right) == nullptr);
}

TEST_CASE("painting draws the picture when there is one", "[image_tile][paint]") {
  fixture f;
  REQUIRE(count_commands<draw_image_command>(f.f.commands()) == 0);
  f.tile->set_image(picture(8, 8));
  REQUIRE(count_commands<draw_image_command>(f.f.commands()) == 1);
}

TEST_CASE("a selected tile draws a border", "[image_tile][paint]") {
  fixture f;
  auto const plain = count_commands<stroke_rounded_rect_command>(f.f.commands());
  f.tile->set_selected(true);
  REQUIRE(count_commands<stroke_rounded_rect_command>(f.f.commands()) == plain + 1);
}

TEST_CASE("a tile is an image named by its description or caption", "[image_tile][accessibility]") {
  fixture f;
  f.tile->set_caption("IMG_0042.jpg");
  REQUIRE(f.tile->accessibility_info().kind == a11y::role::image);
  REQUIRE(f.tile->accessibility_info().name == "IMG_0042.jpg");
  f.tile->set_description("A dog on a beach");
  REQUIRE(f.tile->accessibility_info().name == "A dog on a beach");
  f.tile->set_selected(true);
  REQUIRE(f.tile->accessibility_info().selected);
}
