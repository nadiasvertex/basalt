#pragma once

#include "basalt/layout.h"
#include "basalt/units.h"
#include "basalt/widget.h"

#include <concepts>
#include <memory>
#include <unordered_map>
#include <utility>

namespace basalt {

// Where a `stack` places one child within its content rect.
struct stack_item {
  alignment horizontal = alignment::stretch;
  alignment vertical = alignment::stretch;

  [[nodiscard]] friend constexpr auto operator==(stack_item const &, stack_item const &)
      -> bool = default;
};

// Layers its visible children on top of each other in paint order, each
// aligned within the content rect (bounds inset by `padding`). Children that
// do not stretch keep their measured size, limited to the content rect. Used
// for overlays such as a placeholder over an empty editor or a badge over an
// icon.
class stack : public widget {
public:
  template <std::derived_from<widget> T>
  auto add(std::unique_ptr<T> child, stack_item item) -> T & {
    T &added = add_child(std::move(child));
    set_item(added, item);
    return added;
  }

  [[nodiscard]] auto padding() const -> length_insets const & { return padding_; }
  auto set_padding(length_insets padding) -> void;

  // The default item for children without one.
  [[nodiscard]] auto item(widget const &child) const -> stack_item;
  // False when `child` is not a child of this stack.
  auto set_item(widget const &child, stack_item item) -> bool;

protected:
  auto on_layout() -> void override;
  [[nodiscard]] auto on_measure(constraints const &limits) const -> basalt::size override;
  auto on_child_removed(widget &child) -> void override;

private:
  length_insets padding_;
  std::unordered_map<widget_id, stack_item> items_;
};

} // namespace basalt
