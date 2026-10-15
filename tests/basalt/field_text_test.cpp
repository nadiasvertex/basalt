#include <catch2/catch_test_macros.hpp>

#include "basalt/widgets/field_text.h"

using namespace basalt;

TEST_CASE("field text setters report whether anything changed", "[field_text]") {
  field_text field{"Name", control_size::medium};
  REQUIRE(field.label().text() == "Name");
  REQUIRE_FALSE(field.set_label("Name"));
  REQUIRE(field.set_label("Full name"));
  REQUIRE(field.set_helper("Required"));
  REQUIRE_FALSE(field.set_helper("Required"));
  REQUIRE(field.set_placeholder("Type here"));
  REQUIRE_FALSE(field.set_placeholder("Type here"));
  REQUIRE(field.set_error(true));
  REQUIRE_FALSE(field.set_error(true));
  REQUIRE(field.has_error());
}

TEST_CASE("field text size changes the label and placeholder roles but not the helper",
          "[field_text]") {
  field_text field{"Name", control_size::medium};
  REQUIRE_FALSE(field.set_size(control_size::medium));
  REQUIRE(field.set_size(control_size::large));
  REQUIRE(field.size() == control_size::large);
  REQUIRE(field.label().role() == label_role_for(control_size::large));
  REQUIRE(field.placeholder().role() == label_role_for(control_size::large));
  REQUIRE(field.helper().role() == text_role::small);
}
