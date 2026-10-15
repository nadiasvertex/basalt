#pragma once

#include "basalt/accessibility/role.h"
#include "basalt/geometry.h"
#include "basalt/widget_id.h"

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

namespace basalt::accessibility {

// A semantic node is identified by the widget it describes.
using node_id = widget_id;

// Operations an assistive technology can ask a widget to perform.
enum class action : std::uint8_t {
  // Moves keyboard focus to the widget. Every focusable widget supports it.
  focus,
  // Activates the widget as a click or Enter would.
  press,
  increment,
  decrement,
  expand,
  collapse,
  select,
  show_menu,
  scroll_into_view,
  scroll_up,
  scroll_down,
  scroll_left,
  scroll_right,
  // Replaces the value; `action_request::text` carries it.
  set_value,
  // Moves the text selection; `action_request::selection` carries it.
  set_text_selection,
};

inline constexpr std::size_t action_count =
    static_cast<std::size_t>(action::set_text_selection) + 1;

class action_set {
public:
  constexpr action_set() = default;
  constexpr action_set(std::initializer_list<action> actions) {
    for (auto const a : actions) {
      insert(a);
    }
  }

  constexpr auto insert(action a) -> void { bits_.set(static_cast<std::size_t>(a)); }
  constexpr auto erase(action a) -> void { bits_.reset(static_cast<std::size_t>(a)); }
  [[nodiscard]] constexpr auto contains(action a) const -> bool {
    return bits_.test(static_cast<std::size_t>(a));
  }
  [[nodiscard]] constexpr auto empty() const -> bool { return bits_.none(); }

  [[nodiscard]] friend constexpr auto operator==(action_set const &, action_set const &)
      -> bool = default;

private:
  std::bitset<action_count> bits_;
};

enum class check_state : std::uint8_t { unchecked, checked, mixed };

enum class orientation : std::uint8_t { none, horizontal, vertical };

// How eagerly changes to the node are announced.
enum class live_region : std::uint8_t { off, polite, assertive };

// A selection in UTF-8 byte offsets into the node's `value`. `anchor` is
// where the selection started and `focus` is the caret.
struct text_selection {
  std::size_t anchor = 0;
  std::size_t focus = 0;

  [[nodiscard]] friend constexpr auto operator==(text_selection const &, text_selection const &)
      -> bool = default;
};

struct value_range {
  double minimum = 0;
  double maximum = 100;
  double step = 1;

  [[nodiscard]] friend constexpr auto operator==(value_range const &, value_range const &)
      -> bool = default;
};

// What a widget says about itself. Widgets return this from
// `widget::accessibility_info`; everything the tree knows without asking the
// widget (identity, structure, bounds, focus, enabled state) is added in
// `node`.
struct info {
  role kind = role::none;
  // The accessible name: what the widget is called.
  std::string name;
  std::string description;
  // The current value, such as a text field's contents or a slider's reading.
  std::string value;
  std::string keyboard_shortcut;
  action_set actions;

  std::optional<check_state> checked;
  std::optional<bool> expanded;
  bool selected = false;
  bool read_only = false;
  bool required = false;
  bool multiline = false;
  bool invalid = false;
  bool busy = false;
  basalt::accessibility::orientation orientation = basalt::accessibility::orientation::none;
  live_region live = live_region::off;

  std::optional<value_range> range;
  std::optional<double> numeric_value;
  std::optional<text_selection> selection;

  // Heading level, or nesting depth of a tree item; 0 when not applicable.
  std::uint32_t level = 0;
  // 1-based position within the parent's items, and their count; 0 when not
  // applicable.
  std::uint32_t position_in_set = 0;
  std::uint32_t set_size = 0;

  // Widgets that name or describe this one, used when the name is shown by a
  // separate label widget.
  std::vector<node_id> labelled_by;
  std::vector<node_id> described_by;

  // Orders a node among its siblings; lower comes first and ties keep tree
  // order. The default is tree order.
  int reading_order = 0;

  [[nodiscard]] friend auto operator==(info const &, info const &) -> bool = default;
};

// An accessibility action with its argument.
struct action_request {
  accessibility::action kind = accessibility::action::focus;
  // `action::set_value`.
  std::string text;
  // `action::set_text_selection`.
  text_selection selection;
};

// One node of the semantic tree. Bounds are in the root widget's layout
// pixels.
struct node {
  node_id id;
  // Invalid for the root.
  node_id parent;
  std::vector<node_id> children;
  rect bounds;
  info semantics;

  // Facts the tree adds from widget state.
  bool focusable = false;
  bool focused = false;
  // The widget or an ancestor is disabled. Disabled controls stay focusable.
  bool disabled = false;
  // A modal is active and this node is outside it.
  bool blocked = false;

  [[nodiscard]] auto supports(action a) const -> bool { return semantics.actions.contains(a); }

  [[nodiscard]] friend auto operator==(node const &, node const &) -> bool = default;
};

} // namespace basalt::accessibility
