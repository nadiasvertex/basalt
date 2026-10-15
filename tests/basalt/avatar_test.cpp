#include <catch2/catch_test_macros.hpp>

#include "basalt/widgets/avatar.h"
#include "basalt/widgets/box.h"
#include "basalt/widgets/overlay_host.h"

#include <chrono>
#include <memory>
#include <vector>

#include "input_probe.h"
#include "ui_fixture.h"

using namespace basalt;
using namespace basalt::testing;
using namespace std::chrono_literals;
namespace a11y = basalt::accessibility;

namespace {

auto make_image() -> raster_image_ptr {
  return raster_image::create(2, 2, std::vector<std::uint8_t>(16, 255));
}

struct fixture {
  fixture() {
    host = &f.tree.emplace_root<overlay_host>();
    auto &content = host->set_content(std::make_unique<box>(axis::horizontal));
    content.set_main_alignment(alignment::start);
    content.set_cross_alignment(alignment::start);
    person = &content.emplace_child<avatar>("Ada Lovelace");
    f.layout();
  }

  ui f;
  overlay_host *host = nullptr;
  avatar *person = nullptr;
};

} // namespace

TEST_CASE("initials come from the first and last word", "[avatar]") {
  REQUIRE(avatar_initials("Ada Lovelace") == "AL");
  REQUIRE(avatar_initials("ada king lovelace") == "AL");
  REQUIRE(avatar_initials("  Plato ") == "P");
  REQUIRE(avatar_initials("").empty());
  REQUIRE(avatar_initials("\xC3\x89mile Zola") == "\xC3\x89Z");
}

TEST_CASE("the graphic size follows the control size", "[avatar]") {
  fixture f;
  f.person->set_size(control_size::small);
  float const small = f.person->diameter();
  f.person->set_size(control_size::large);
  REQUIRE(f.person->diameter() > small);
}

TEST_CASE("the graphic-only layout is just the circle", "[avatar]") {
  fixture f;
  f.person->set_supporting_text("Mathematician");
  f.f.layout();
  float const with_text = f.person->bounds().width();
  f.person->set_layout(avatar_layout::graphic_only);
  f.f.layout();
  REQUIRE(f.person->bounds().width() == f.person->diameter());
  REQUIRE(f.person->bounds().width() < with_text + 0.5f);
}

TEST_CASE("an image replaces the initials", "[avatar]") {
  fixture f;
  f.person->set_layout(avatar_layout::graphic_only);
  f.f.layout();
  REQUIRE(count_commands<draw_image_command>(f.f.commands()) == 0);
  REQUIRE(count_commands<fill_rounded_rect_command>(f.f.commands()) >= 1);
  f.person->set_image(make_image());
  REQUIRE(count_commands<draw_image_command>(f.f.commands()) == 1);
}

TEST_CASE("the graphic-only layout shows the name in a tooltip", "[avatar]") {
  fixture f;
  f.person->set_layout(avatar_layout::graphic_only);
  f.f.layout();
  f.f.send(move(f.person->bounds().center()));
  f.f.advance(300ms);
  f.f.advance(300ms);
  REQUIRE(f.host->overlay_count() == 1);
  f.f.send(move({390, 290}));
  REQUIRE(f.host->overlay_count() == 0);
}

TEST_CASE("text layouts show no tooltip", "[avatar]") {
  fixture f;
  f.person->set_layout(avatar_layout::graphic_only);
  f.person->set_layout(avatar_layout::trailing);
  f.f.layout();
  f.f.send(move(f.person->bounds().center()));
  f.f.advance(300ms);
  f.f.advance(300ms);
  REQUIRE(f.host->overlay_count() == 0);
}

TEST_CASE("avatars report the name and supporting text", "[avatar][accessibility]") {
  fixture f;
  f.person->set_supporting_text("Mathematician");
  auto const snapshot = f.f.semantics();
  auto const *node = snapshot.find(f.person->id());
  REQUIRE(node->semantics.kind == a11y::role::group);
  REQUIRE(node->semantics.name == "Ada Lovelace");
  REQUIRE(node->semantics.description == "Mathematician");

  f.person->set_layout(avatar_layout::graphic_only);
  REQUIRE(f.f.semantics().find(f.person->id())->semantics.kind == a11y::role::image);
  REQUIRE(a11y::audit(f.f.semantics()).empty());
}
