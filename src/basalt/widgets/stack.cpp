#include "basalt/widgets/stack.h"

#include <algorithm>
#include <utility>

namespace basalt {

auto stack::set_padding(length_insets padding) -> void {
  if (padding != padding_) {
    padding_ = padding;
    invalidate_layout();
  }
}

auto stack::item(widget const &child) const -> stack_item {
  auto const it = items_.find(child.id());
  return it == items_.end() ? stack_item{} : it->second;
}

auto stack::set_item(widget const &child, stack_item item) -> bool {
  if (child.parent() != this) {
    return false;
  }
  auto &slot = items_[child.id()];
  if (slot != item) {
    slot = item;
    invalidate_layout();
  }
  return true;
}

auto stack::on_child_removed(widget &child) -> void { items_.erase(child.id()); }

auto stack::on_measure(constraints const &limits) const -> basalt::size {
  insets const pad = resolve(padding_);
  constraints const each = constraints::loose(limits.deflate(pad).max);
  basalt::size wanted;
  for (auto const &c : children()) {
    if (c.visible()) {
      basalt::size const measured = c.measure(each);
      wanted = {std::max(wanted.width, measured.width), std::max(wanted.height, measured.height)};
    }
  }
  return inflate(wanted, pad);
}

auto stack::on_layout() -> void {
  rect const content = inset({{}, bounds().extent}, resolve(padding_));
  constraints const each = constraints::loose(content.extent);
  scale_factors const factors = scale();
  auto const place = [](alignment a, float wanted, float available) {
    float const extent = a == alignment::stretch ? available : std::min(wanted, available);
    return std::pair{align_offset(a, available - extent), extent};
  };
  for (auto &c : children()) {
    if (!c.visible()) {
      continue;
    }
    stack_item const it = item(c);
    basalt::size const measured = c.measure(each);
    auto const [x, width] = place(it.horizontal, measured.width, content.width());
    auto const [y, height] = place(it.vertical, measured.height, content.height());
    c.set_bounds(snap_to_device(rect{content.origin + point{x, y}, {width, height}}, factors));
  }
}

} // namespace basalt
