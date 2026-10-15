#include <catch2/catch_test_macros.hpp>

#include "basalt/widget_tree.h"
#include "basalt/widgets/box.h"

#include <memory>
#include <utility>

#include "leaf.h"

using namespace basalt;
using basalt::testing::leaf;

namespace {

struct fixture {
  explicit fixture(axis direction = axis::vertical, scale_factors scale = {}) : tree({}, scale) {
    tree.set_viewport({200, 100});
    root = &tree.emplace_root<box>(direction);
  }

  widget_tree tree;
  box *root = nullptr;
};

} // namespace

TEST_CASE("a column stacks children with gap and padding", "[layout][box]") {
  fixture f;
  f.root->set_padding(length_insets::uniform(fixed(10)));
  f.root->set_gap(fixed(5));
  auto &a = f.root->emplace_child<leaf>(size{50, 20});
  auto &b = f.root->emplace_child<leaf>(size{80, 30});
  f.tree.update_layout();

  REQUIRE(a.bounds() == rect{{10, 10}, {180, 20}});
  REQUIRE(b.bounds() == rect{{10, 35}, {180, 30}});
  REQUIRE(f.root->measure({}) == size{100, 75});

  SECTION("cross alignment keeps the measured width") {
    f.root->set_cross_alignment(alignment::center);
    f.root->set_item(b, {.align = alignment::end});
    f.tree.update_layout();
    REQUIRE(a.bounds() == rect{{75, 10}, {50, 20}});
    REQUIRE(b.bounds() == rect{{110, 35}, {80, 30}});
  }

  SECTION("main alignment places the spare space") {
    f.root->set_main_alignment(alignment::end);
    f.tree.update_layout();
    REQUIRE(a.bounds().top() == 35);
    REQUIRE(b.bounds().top() == 60);
  }

  SECTION("hidden children take no space") {
    a.set_visible(false);
    f.tree.update_layout();
    REQUIRE(b.bounds() == rect{{10, 10}, {180, 30}});
    REQUIRE(f.root->measure({}) == size{100, 50});
  }

  SECTION("a child that changes its wanted size moves its siblings") {
    a.set_wanted({50, 40});
    REQUIRE(f.tree.needs_layout());
    f.tree.update_layout();
    REQUIRE(b.bounds().top() == 55);
  }
}

TEST_CASE("a row grows and shrinks children along the main axis", "[layout][box]") {
  fixture f(axis::horizontal);
  auto &a = f.root->add(std::make_unique<leaf>(size{20, 10}), {.grow = 1});
  auto &b = f.root->add(std::make_unique<leaf>(size{20, 10}), {.grow = 3});
  auto &c = f.root->emplace_child<leaf>(size{40, 10});
  f.tree.update_layout();

  REQUIRE(a.bounds() == rect{{0, 0}, {50, 100}});
  REQUIRE(b.bounds() == rect{{50, 0}, {110, 100}});
  REQUIRE(c.bounds() == rect{{160, 0}, {40, 100}});

  SECTION("basis and limits override the measured size") {
    f.root->set_item(a, {.basis = fixed(100), .max = fixed(60)});
    f.root->set_item(b, {.grow = 1, .min = fixed(30)});
    f.tree.update_layout();
    REQUIRE(a.bounds().width() == 60);
    REQUIRE(b.bounds().width() == 100);
  }

  SECTION("children shrink when the row is too narrow") {
    f.tree.set_viewport({40, 100});
    f.tree.update_layout();
    REQUIRE(a.bounds().width() == 10);
    REQUIRE(b.bounds().width() == 10);
    REQUIRE(c.bounds().width() == 20);
  }
}

TEST_CASE("box items belong to current children only", "[layout][box]") {
  fixture f;
  auto &a = f.root->add(std::make_unique<leaf>(), {.grow = 2});
  REQUIRE(f.root->item(a).grow == 2);

  box other;
  REQUIRE_FALSE(other.set_item(a, {.grow = 1}));

  auto owned = f.root->remove_child(a);
  auto &again = f.root->add_child(std::move(owned));
  REQUIRE(f.root->item(again) == box_item{});
}

TEST_CASE("box lengths follow zoom and text scale", "[layout][box][scaling]") {
  auto build = [](fixture &f) -> leaf & {
    f.root->set_padding(length_insets::uniform(relative(8)));
    f.root->set_gap(fixed(4));
    f.root->emplace_child<leaf>(size{10, 10});
    return f.root->emplace_child<leaf>(size{10, 10});
  };

  SECTION("200% text scales relative padding but not the fixed gap") {
    fixture f(axis::vertical, {.text = 2});
    auto &second = build(f);
    f.tree.update_layout();
    REQUIRE(second.bounds() == rect{{16, 30}, {168, 10}});
  }

  SECTION("200% zoom scales every length") {
    fixture f(axis::vertical, {.zoom = 2});
    auto &second = build(f);
    f.tree.update_layout();
    REQUIRE(second.bounds() == rect{{16, 34}, {168, 10}});
  }

  SECTION("changing the scale relays out the tree") {
    fixture f;
    auto &second = build(f);
    f.tree.update_layout();
    REQUIRE(second.bounds().top() == 22);
    f.tree.set_scale({.text = 2});
    f.tree.update_layout();
    REQUIRE(second.bounds().top() == 30);
  }
}

TEST_CASE("box edges snap to device pixels", "[layout][box][scaling]") {
  auto layout = [](scale_factors scale) {
    fixture f(axis::horizontal, scale);
    f.tree.set_viewport({10, 10});
    for (int i = 0; i < 4; ++i) {
      f.root->add(std::make_unique<leaf>(), {.grow = 1});
    }
    f.tree.update_layout();
    return std::pair{f.root->child(1).bounds(), f.root->child(2).bounds()};
  };

  auto const [one_b, one_c] = layout({});
  REQUIRE(one_b.left() == 3);
  REQUIRE(one_b.right() == one_c.left());

  auto const [two_b, two_c] = layout({.display = 2});
  REQUIRE(two_b.left() == 2.5f);
  REQUIRE(two_b.right() == 5);
  REQUIRE(two_c.right() == 7.5f);
}

namespace {

// Counts how often it is measured.
struct metered : leaf {
  using leaf::leaf;
  mutable int measures = 0;

protected:
  [[nodiscard]] auto on_measure(constraints const &limits) const -> size override {
    ++measures;
    return leaf::on_measure(limits);
  }
};

} // namespace

TEST_CASE("a stretched child with a basis is not measured during layout", "[layout][box]") {
  fixture f(axis::horizontal);
  auto &sized =
      f.root->add(std::make_unique<metered>(size{50, 20}), {.grow = 1, .basis = fixed(0)});
  auto &natural = f.root->add(std::make_unique<metered>(size{30, 20}), {});
  auto &centered = f.root->add(std::make_unique<metered>(size{20, 10}),
                               {.basis = fixed(20), .align = alignment::center});
  f.tree.update_layout();

  CHECK(sized.measures == 0);
  CHECK(natural.measures > 0);
  CHECK(centered.measures > 0);
  CHECK(sized.bounds() == rect{{0, 0}, {150, 100}});
  CHECK(centered.bounds().height() == 10);
}
