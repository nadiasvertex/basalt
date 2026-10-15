#include "basalt/widgets/status_bar.h"

#include "basalt/render_context.h"
#include "basalt/widget_tree.h"

#include <memory>
#include <utility>

namespace basalt {

status_bar::status_bar(std::string name) : box(axis::horizontal), name_(std::move(name)) {
  set_padding(length_insets::symmetric(fixed(12), fixed(4)));
  set_gap(fixed(16));
  set_cross_alignment(alignment::center);
}

auto status_bar::add_section(std::string id, std::string text, float grow) -> label & {
  auto &added = add(std::make_unique<label>(std::move(text), text_role::small), {.grow = grow});
  sections_[std::move(id)] = added.id();
  return added;
}

auto status_bar::section(std::string const &id) const -> label * {
  auto const found = sections_.find(id);
  if (found == sections_.end()) {
    return nullptr;
  }
  for (widget &child : children()) {
    if (child.id() == found->second) {
      return static_cast<label *>(&child);
    }
  }
  return nullptr;
}

auto status_bar::set_section_text(std::string const &id, std::string text) -> bool {
  label *const target = section(id);
  if (target == nullptr) {
    return false;
  }
  target->set_text(std::move(text));
  return true;
}

auto status_bar::accessibility_info() const -> accessibility::info {
  accessibility::info details;
  details.kind = accessibility::role::status_bar;
  details.name = name_;
  details.live = accessibility::live_region::polite;
  return details;
}

auto status_bar::on_paint(render_context &context) const -> void {
  if (tree() == nullptr) {
    return;
  }
  auto const &colors = theme().colors;
  rect const area{{}, bounds().extent};
  context.fill_rect(area, colors.background_secondary);
  float const width = device_stroke_width(fixed(1), scale());
  context.fill_rect({{0, 0}, {area.width(), width}}, colors.border_subdued);
}

} // namespace basalt
