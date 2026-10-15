#include <catch2/catch_test_macros.hpp>

#include "basalt/event.h"
#include "basalt/input.h"

using namespace basalt;

TEST_CASE("modifier sets combine and query", "[input]") {
  auto const set = modifier::shift | modifier::meta;
  REQUIRE(has(set, modifier::shift));
  REQUIRE(has(set, modifier::meta));
  REQUIRE_FALSE(has(set, modifier::alt));
  REQUIRE_FALSE(has(set, modifier::none));
  REQUIRE(has(set, modifier::shift | modifier::meta));
}

TEST_CASE("modifier keys map to modifiers", "[input]") {
  REQUIRE(modifier_of(key::right_control) == modifier::control);
  REQUIRE(modifier_of(key::left_meta) == modifier::meta);
  REQUIRE(modifier_of(key::a) == modifier::none);
  REQUIRE(is_modifier_key(key::caps_lock));
  REQUIRE_FALSE(is_modifier_key(key::enter));
}

TEST_CASE("every key has a name", "[input]") {
  REQUIRE(key_name(key::a) == "a");
  REQUIRE(key_name(key::digit_9) == "9");
  REQUIRE(key_name(key::delete_forward) == "delete");
  REQUIRE(key_name(key::menu) == "menu");
  REQUIRE(key_name(static_cast<key>(9999)) == "unknown");
}

TEST_CASE("pointer button sets track held buttons", "[input]") {
  auto held = pointer_button::primary;
  held |= pointer_button::middle;
  REQUIRE(has(held, pointer_button::middle));
  REQUIRE_FALSE(has(held, pointer_button::secondary));
}

TEST_CASE("events expose time, position and routing class", "[input]") {
  using namespace std::chrono_literals;
  event const down = pointer_event{.action = pointer_action::down,
                                   .position = {3, 4},
                                   .button = pointer_button::primary,
                                   .time = 5ms};
  event const wheel = wheel_event{.position = {7, 8}, .delta = {0, 1}, .time = 6ms};
  event const typed = text_input_event{.text = "é", .time = 7ms};
  event const command = command_event{.id = "file.save"};

  REQUIRE(event_time_of(down) == 5ms);
  REQUIRE(event_position(down) == point{3, 4});
  REQUIRE(event_position(wheel) == point{7, 8});
  REQUIRE_FALSE(event_position(typed).has_value());
  REQUIRE(is_keyboard_event(typed));
  REQUIRE(is_keyboard_event(key_event{}));
  REQUIRE_FALSE(is_keyboard_event(down));
  REQUIRE_FALSE(is_keyboard_event(command));
}
