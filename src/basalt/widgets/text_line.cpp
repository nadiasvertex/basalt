#include "basalt/widgets/text_line.h"

#include "basalt/widget_tree.h"

#include <algorithm>
#include <utility>

namespace basalt {

auto text_style_for(theme const &t, text_role role) -> text_style {
  switch (role) {
  case text_role::body:
    break;
  case text_role::body_strong:
    return t.text.body_semibold_medium;
  case text_role::small:
    return t.text.body_regular_small;
  case text_role::small_strong:
    return t.text.body_semibold_small;
  case text_role::large:
    return t.text.body_regular_large;
  case text_role::heading_small:
    return t.text.heading_small;
  case text_role::heading_medium:
    return t.text.heading_medium;
  case text_role::heading_large:
    return t.text.heading_large;
  case text_role::reading:
    return t.reading_text;
  case text_role::source:
    return t.source_text;
  }
  return t.text.body_regular_medium;
}

auto text_line::set_text(std::string text) -> bool {
  if (text == text_) {
    return false;
  }
  text_ = std::move(text);
  cache_.valid = false;
  return true;
}

auto text_line::set_role(text_role role) -> bool {
  if (role == role_) {
    return false;
  }
  role_ = role;
  cache_.valid = false;
  return true;
}

auto text_line::shaped(widget const &owner) const -> cache const & {
  widget_tree const *const tree = owner.tree();
  text::text_shaper const *const shaper = tree != nullptr ? tree->text_shaper() : nullptr;
  text_style const style = to_layout(text_style_for(owner.theme(), role_), owner.scale());
  if (cache_.valid && cache_.shaper == shaper && cache_.style == style) {
    return cache_;
  }
  cache_ = {.shaper = shaper, .style = style, .valid = true};
  if (shaper != nullptr && !text_.empty()) {
    cache_.shaped = shaper->shape(text_, {.style = style});
  }
  cache_.line_height = style.line_height;
  if (cache_.line_height <= 0) {
    cache_.line_height = style.font_size * 1.25f;
  }
  return cache_;
}

auto text_line::measure(widget const &owner) const -> basalt::size {
  cache const &c = shaped(owner);
  return {c.shaped.width, std::max(c.line_height, c.shaped.ascent + c.shaped.descent)};
}

auto text_line::draw(render_context &context, widget const &owner, point origin, rgba color) const
    -> void {
  cache const &c = shaped(owner);
  if (c.shaped.runs.empty()) {
    return;
  }
  float const height = std::max(c.line_height, c.shaped.ascent + c.shaped.descent);
  float const baseline =
      origin.y + (height - (c.shaped.ascent + c.shaped.descent)) / 2 + c.shaped.ascent;
  float const y = snap_to_device(baseline, owner.scale());
  for (auto const &placed : c.shaped.runs) {
    context.draw_glyph_run({origin.x + placed.x, y}, placed.run, color);
  }
}

auto text_line::draw_in(render_context &context, widget const &owner, rect area, rgba color,
                        alignment horizontal, alignment vertical) const -> void {
  basalt::size const natural = measure(owner);
  point const origin{area.left() + align_offset(horizontal, area.width() - natural.width),
                     area.top() + align_offset(vertical, area.height() - natural.height)};
  draw(context, owner, origin, color);
}

} // namespace basalt
