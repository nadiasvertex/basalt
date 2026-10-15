#include <catch2/catch_test_macros.hpp>

#include "basalt/widget_tree.h"
#include "basalt/widgets/stack.h"

#include <memory>

#include "leaf.h"

using namespace basalt;
using basalt::testing::leaf;

TEST_CASE("a stack layers children within its padding", "[layout][stack]") {
  widget_tree tree;
  tree.set_viewport({100, 80});
  auto &root = tree.emplace_root<stack>();
  root.set_padding(length_insets::uniform(fixed(10)));
  auto &back = root.emplace_child<leaf>(size{20, 20});
  auto &badge = root.add(std::make_unique<leaf>(size{20, 10}),
                         {.horizontal = alignment::end, .vertical = alignment::start});
  auto &centered = root.add(std::make_unique<leaf>(size{40, 200}),
                            {.horizontal = alignment::center, .vertical = alignment::center});
  tree.update_layout();

  REQUIRE(back.bounds() == rect{{10, 10}, {80, 60}});
  REQUIRE(badge.bounds() == rect{{70, 10}, {20, 10}});
  // Too tall for the content rect, so it is limited to it.
  REQUIRE(centered.bounds() == rect{{30, 10}, {40, 60}});

  REQUIRE(root.measure({}) == size{60, 220});
  REQUIRE(root.item(back) == stack_item{});

  SECTION("hidden children are not measured") {
    centered.set_visible(false);
    REQUIRE(root.measure({}) == size{40, 40});
  }
}
