#include "basalt/widgets/text_field.h"

#include "basalt/event_router.h"
#include "basalt/render_context.h"
#include "basalt/widget_tree.h"
#include "basalt/widgets/scroll_view.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <limits>
#include <utility>
#include <variant>

#include "notify.h"
#include "scroll_into_view.h"

namespace basalt {

namespace {

enum class char_class : std::uint8_t { space, word, punctuation };

// Bytes of multi-byte characters count as word characters, so words in any
// script stay whole.
auto classify(char c) -> char_class {
  auto const u = static_cast<unsigned char>(c);
  if (u == ' ' || u == '\t' || u == '\n' || u == '\r') {
    return char_class::space;
  }
  if (u >= 0x80 || u == '_' || (u >= '0' && u <= '9') || (u >= 'a' && u <= 'z') ||
      (u >= 'A' && u <= 'Z')) {
    return char_class::word;
  }
  return char_class::punctuation;
}

// Line breaks in text pasted into a single-line field become spaces.
auto flatten_line_breaks(std::string_view text) -> std::string {
  std::string flat;
  flat.reserve(text.size());
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n') {
      continue;
    }
    flat.push_back(text[i] == '\n' || text[i] == '\r' ? ' ' : text[i]);
  }
  return flat;
}

} // namespace

text_field::text_field(std::string text, bool multiline)
    : text_(std::move(text)), placeholder_({}, text_role::body), multiline_(multiline) {
  if (!multiline_) {
    text_ = flatten_line_breaks(text_);
  }
  anchor_ = caret_ = text_.size();
  set_focus_policy(focus_policy::strong);
  set_minimum_target(true);
}

auto text_field::boundaries() const -> std::vector<std::size_t> const & {
  if (!boundaries_valid_) {
    boundaries_ = text::default_grapheme_boundaries(text_);
    if (boundaries_.empty() || boundaries_.front() != 0) {
      boundaries_.insert(boundaries_.begin(), 0);
    }
    if (boundaries_.back() != text_.size()) {
      boundaries_.push_back(text_.size());
    }
    boundaries_valid_ = true;
  }
  return boundaries_;
}

auto text_field::snap(std::size_t offset) const -> std::size_t {
  offset = std::min(offset, text_.size());
  auto const &b = boundaries();
  auto const it = std::upper_bound(b.begin(), b.end(), offset);
  return it == b.begin() ? 0 : *std::prev(it);
}

auto text_field::previous_boundary(std::size_t offset, unit_of_move unit) const -> std::size_t {
  auto const &b = boundaries();
  auto const step = [&](std::size_t from) {
    auto const it = std::lower_bound(b.begin(), b.end(), from);
    return it == b.begin() ? std::size_t{0} : *std::prev(it);
  };
  switch (unit) {
  case unit_of_move::grapheme:
    return step(offset);
  case unit_of_move::word: {
    std::size_t pos = offset;
    while (pos > 0 && classify(text_[pos - 1]) == char_class::space) {
      --pos;
    }
    if (pos > 0) {
      char_class const kind = classify(text_[pos - 1]);
      while (pos > 0 && classify(text_[pos - 1]) == kind) {
        --pos;
      }
    }
    return snap(pos);
  }
  case unit_of_move::line:
  case unit_of_move::document:
    break;
  }
  return 0;
}

auto text_field::next_boundary(std::size_t offset, unit_of_move unit) const -> std::size_t {
  auto const &b = boundaries();
  switch (unit) {
  case unit_of_move::grapheme: {
    auto const it = std::upper_bound(b.begin(), b.end(), offset);
    return it == b.end() ? text_.size() : *it;
  }
  case unit_of_move::word: {
    std::size_t pos = offset;
    while (pos < text_.size() && classify(text_[pos]) == char_class::space) {
      ++pos;
    }
    if (pos < text_.size()) {
      char_class const kind = classify(text_[pos]);
      while (pos < text_.size() && classify(text_[pos]) == kind) {
        ++pos;
      }
    }
    return snap(pos);
  }
  case unit_of_move::line:
  case unit_of_move::document:
    break;
  }
  return text_.size();
}

auto text_field::line_bounds(std::size_t offset) const -> std::pair<std::size_t, std::size_t> {
  if (!multiline_) {
    return {0, text_.size()};
  }
  auto const before = text_.rfind('\n', offset == 0 ? std::string::npos : offset - 1);
  auto const after = text_.find('\n', offset);
  return {before == std::string::npos || offset == 0 ? 0 : before + 1,
          after == std::string::npos ? text_.size() : after};
}

auto text_field::selection_begin() const -> std::size_t { return std::min(anchor_, caret_); }
auto text_field::selection_end() const -> std::size_t { return std::max(anchor_, caret_); }

auto text_field::selected_text() const -> std::string_view {
  return std::string_view(text_).substr(selection_begin(), selection_end() - selection_begin());
}

auto text_field::padding() const -> insets {
  if (!framed_) {
    return {};
  }
  auto const &sizes = theme().sizes;
  return insets::symmetric(resolve(sizes.inset_horizontal_input),
                           resolve(sizes.inset_vertical_text_area));
}

auto text_field::content_width() const -> float {
  return multiline_ ? std::max(0.0f, bounds().width() - padding().horizontal()) : unbounded;
}

auto text_field::line_height() const -> float {
  text_style const style = to_layout(text_style_for(theme(), role_), scale());
  return style.line_height > 0 ? style.line_height : style.font_size * 1.25f;
}

auto text_field::content_origin() const -> point {
  insets const pad = padding();
  return {pad.left - (multiline_ ? 0 : scroll_x_), pad.top};
}

auto text_field::display_text() const -> std::string {
  if (preedit_.empty()) {
    return text_;
  }
  std::string shown = text_;
  shown.insert(caret_, preedit_);
  return shown;
}

auto text_field::display_caret() const -> std::size_t { return caret_ + preedit_.size(); }

auto text_field::from_display(std::size_t offset) const -> std::size_t {
  if (preedit_.empty() || offset <= caret_) {
    return std::min(offset, text_.size());
  }
  return offset >= caret_ + preedit_.size() ? offset - preedit_.size() : caret_;
}

auto text_field::paragraph_for(float width) const -> text::paragraph const * {
  widget_tree const *const t = tree();
  text::text_shaper const *const shaper = t != nullptr ? t->text_shaper() : nullptr;
  if (shaper == nullptr) {
    return nullptr;
  }
  text_style const style = to_layout(text_style_for(theme(), role_), scale());
  std::string shown = display_text();
  if (!cache_.valid || cache_.shaper != shaper || cache_.style != style || cache_.width != width ||
      cache_.wrap != multiline_ || cache_.text != shown) {
    text::paragraph_options options;
    options.shape.style = style;
    options.wrap = multiline_;
    options.max_width = width;
    cache_ = {.shaper = shaper,
              .style = style,
              .text = shown,
              .width = width,
              .wrap = multiline_,
              .laid_out = std::make_shared<text::paragraph const>(*shaper, shown, options),
              .valid = true};
  }
  return cache_.laid_out.get();
}

auto text_field::caret_rect() const -> rect {
  point const origin = content_origin();
  if (text::paragraph const *const p = paragraph_for(content_width())) {
    return translated(p->caret_rect(display_caret(), affinity_), origin);
  }
  float const height = line_height();
  std::size_t const row =
      multiline_ ? static_cast<std::size_t>(std::count(text_.begin(), text_.begin() + caret_, '\n'))
                 : 0;
  return {{origin.x, origin.y + static_cast<float>(row) * height}, {0, height}};
}

auto text_field::offset_at(point position) const -> std::size_t {
  point const local = position - content_origin();
  if (text::paragraph const *const p = paragraph_for(content_width())) {
    return from_display(p->hit_test(local).offset);
  }
  return local.x <= 0 ? 0 : text_.size();
}

auto text_field::set_text(std::string text) -> void {
  if (!multiline_) {
    text = flatten_line_breaks(text);
  }
  if (text == text_ && preedit_.empty()) {
    return;
  }
  text_ = std::move(text);
  boundaries_valid_ = false;
  preedit_.clear();
  anchor_ = caret_ = text_.size();
  goal_x_.reset();
  affinity_ = text::caret_affinity::downstream;
  scroll_x_ = 0;
  invalidate_content();
}

auto text_field::set_multiline(bool multiline) -> void {
  if (multiline == multiline_) {
    return;
  }
  multiline_ = multiline;
  if (!multiline_) {
    auto const flat = flatten_line_breaks(text_);
    if (flat != text_) {
      set_text(flat);
    }
  }
  scroll_x_ = 0;
  invalidate_content();
}

auto text_field::set_placeholder(std::string placeholder) -> void {
  if (placeholder_.set_text(std::move(placeholder))) {
    invalidate_paint();
  }
}

auto text_field::set_read_only(bool read_only) -> void {
  if (read_only != read_only_) {
    read_only_ = read_only;
    set_preedit({});
    invalidate_paint();
  }
}

auto text_field::set_role(text_role role) -> void {
  if (role != role_) {
    role_ = role;
    invalidate_content();
  }
}

auto text_field::set_framed(bool framed) -> void {
  if (framed != framed_) {
    framed_ = framed;
    invalidate_content();
  }
}

auto text_field::set_preferred_width(length width) -> void {
  if (width != preferred_width_) {
    preferred_width_ = width;
    invalidate_layout();
  }
}

auto text_field::set_selection(std::size_t anchor, std::size_t focus) -> void {
  anchor_ = snap(anchor);
  caret_ = snap(focus);
  goal_x_.reset();
  affinity_ = text::caret_affinity::downstream;
  selection_changed();
}

auto text_field::select_all() -> void { set_selection(0, text_.size()); }

auto text_field::selection_changed() -> void {
  invalidate_paint();
  reveal_caret();
}

auto text_field::reveal_caret() -> void {
  if (tree() == nullptr) {
    return;
  }
  rect const caret = caret_rect();
  if (!multiline_) {
    float const visible = bounds().width() - padding().horizontal();
    if (visible <= 0) {
      return;
    }
    float const x = caret.left() - content_origin().x;
    float const before = scroll_x_;
    if (x < scroll_x_) {
      scroll_x_ = x;
    } else if (x + 1 > scroll_x_ + visible) {
      scroll_x_ = x + 1 - visible;
    }
    float limit = 0;
    if (text::paragraph const *const p = paragraph_for(unbounded)) {
      limit = std::max(0.0f, p->extent().width + 1 - visible);
    }
    scroll_x_ = std::clamp(scroll_x_, 0.0f, limit);
    if (scroll_x_ != before) {
      invalidate_paint();
    }
    return;
  }
  scroll_into_view_of(*this, caret);
}

auto text_field::notify_change() -> void {
  // The handler may destroy this field, so it gets a copy of the text.
  auto const value = text_;
  notify(on_change_, value);
}

auto text_field::replace_range(std::size_t begin, std::size_t end, std::string_view replacement)
    -> void {
  text_.replace(begin, end - begin, replacement);
  boundaries_valid_ = false;
  preedit_.clear();
  anchor_ = caret_ = begin + replacement.size();
  goal_x_.reset();
  affinity_ = text::caret_affinity::downstream;
  invalidate_content();
  reveal_caret();
  notify_change();
}

auto text_field::insert_text(std::string_view text) -> bool {
  if (read_only_) {
    return false;
  }
  if (multiline_) {
    replace_range(selection_begin(), selection_end(), text);
  } else {
    replace_range(selection_begin(), selection_end(), flatten_line_breaks(text));
  }
  return true;
}

auto text_field::set_preedit(std::string preedit) -> void {
  if (preedit == preedit_) {
    return;
  }
  preedit_ = std::move(preedit);
  invalidate_content();
  reveal_caret();
}

auto text_field::move_caret(std::size_t to, bool extend) -> void {
  caret_ = snap(to);
  if (!extend) {
    anchor_ = caret_;
  }
  goal_x_.reset();
  affinity_ = text::caret_affinity::downstream;
  selection_changed();
}

auto text_field::move_vertically(int direction, bool extend) -> bool {
  text::paragraph const *const p = paragraph_for(content_width());
  if (p == nullptr) {
    return false;
  }
  if (!preedit_.empty()) {
    return true;
  }
  std::size_t const line = p->line_of(caret_, affinity_);
  bool const first = direction < 0 && line == 0;
  bool const last = direction > 0 && line + 1 >= p->lines().size();
  if (first || last) {
    move_caret(first ? 0 : text_.size(), extend);
    return true;
  }
  float const x = goal_x_.value_or(p->caret_rect(caret_, affinity_).left());
  text::text_hit const hit = p->hit_test_line(direction < 0 ? line - 1 : line + 1, x);
  caret_ = snap(hit.offset);
  if (!extend) {
    anchor_ = caret_;
  }
  affinity_ = hit.affinity;
  goal_x_ = x;
  selection_changed();
  return true;
}

auto text_field::handle_key(key_event const &k) -> bool {
  bool const shift = has(k.modifiers, modifier::shift);
  bool const alt = has(k.modifiers, modifier::alt);
  bool const ctrl = has(k.modifiers, modifier::control);
  bool const meta = has(k.modifiers, modifier::meta);
  bool const shortcut = (ctrl || meta) && !alt;
  unit_of_move const reach = (alt || ctrl) && !meta ? unit_of_move::word : unit_of_move::grapheme;

  switch (k.code) {
  case key::left:
  case key::right: {
    bool const forward = k.code == key::right;
    if (meta && !alt && !ctrl) {
      auto const [begin, end] = line_bounds(caret_);
      if (text::paragraph const *const p = paragraph_for(content_width()); p != nullptr) {
        auto const &line = p->lines()[p->line_of(display_caret(), affinity_)];
        move_caret(from_display(forward ? line.end : line.begin), shift);
        if (forward && line.wrapped) {
          affinity_ = text::caret_affinity::upstream;
        }
        return true;
      }
      move_caret(forward ? end : begin, shift);
      return true;
    }
    if (!shift && has_selection()) {
      move_caret(forward ? selection_end() : selection_begin(), false);
    } else {
      move_caret(forward ? next_boundary(caret_, reach) : previous_boundary(caret_, reach), shift);
    }
    return true;
  }
  case key::up:
  case key::down:
    return multiline_ && move_vertically(k.code == key::down ? 1 : -1, shift);
  case key::home:
  case key::end: {
    bool const forward = k.code == key::end;
    if (ctrl || meta) {
      move_caret(forward ? text_.size() : 0, shift);
      return true;
    }
    if (text::paragraph const *const p = paragraph_for(content_width()); p != nullptr) {
      auto const &line = p->lines()[p->line_of(display_caret(), affinity_)];
      move_caret(from_display(forward ? line.end : line.begin), shift);
      if (forward && line.wrapped) {
        affinity_ = text::caret_affinity::upstream;
      }
      return true;
    }
    auto const [begin, end] = line_bounds(caret_);
    move_caret(forward ? end : begin, shift);
    return true;
  }
  case key::backspace:
  case key::delete_forward: {
    if (read_only_) {
      return false;
    }
    if (has_selection()) {
      replace_range(selection_begin(), selection_end(), {});
      return true;
    }
    bool const forward = k.code == key::delete_forward;
    std::size_t const other =
        forward ? next_boundary(caret_, reach) : previous_boundary(caret_, reach);
    if (other != caret_) {
      replace_range(std::min(other, caret_), std::max(other, caret_), {});
    }
    return true;
  }
  case key::enter:
    if (multiline_) {
      return insert_text("\n");
    }
    if (on_submit_) {
      auto const handler = on_submit_;
      auto const value = text_;
      handler(value);
      return true;
    }
    return false;
  case key::a:
    if (!shortcut) {
      return false;
    }
    select_all();
    return true;
  case key::c:
  case key::x:
    if (!shortcut || !clipboard_.write) {
      return false;
    }
    if (has_selection()) {
      clipboard_.write(std::string(selected_text()));
      if (k.code == key::x && !read_only_) {
        replace_range(selection_begin(), selection_end(), {});
      }
    }
    return true;
  case key::v:
    if (!shortcut || read_only_ || !clipboard_.read) {
      return false;
    }
    insert_text(clipboard_.read());
    return true;
  default:
    return false;
  }
}

auto text_field::handle_pointer(pointer_event const &p) -> bool {
  switch (p.action) {
  case pointer_action::down: {
    if (p.button != pointer_button::primary) {
      return false;
    }
    point const local = p.position - content_origin();
    std::size_t offset = 0;
    text::caret_affinity affinity = text::caret_affinity::downstream;
    if (text::paragraph const *const para = paragraph_for(content_width())) {
      text::text_hit const hit = para->hit_test(local);
      offset = from_display(hit.offset);
      affinity = hit.affinity;
    } else {
      offset = local.x <= 0 ? 0 : text_.size();
    }
    offset = snap(offset);
    if (p.click_count == 2 && !text_.empty()) {
      std::size_t begin = offset;
      std::size_t end = offset;
      char_class const kind = classify(text_[std::min(offset, text_.size() - 1)]);
      while (begin > 0 && classify(text_[begin - 1]) == kind) {
        --begin;
      }
      while (end < text_.size() && classify(text_[end]) == kind) {
        ++end;
      }
      anchor_ = snap(begin);
      caret_ = snap(end);
    } else if (p.click_count >= 3) {
      auto const [begin, end] = line_bounds(offset);
      anchor_ = begin;
      caret_ = end;
    } else {
      caret_ = offset;
      if (!has(p.modifiers, modifier::shift)) {
        anchor_ = offset;
      }
    }
    affinity_ = affinity;
    goal_x_.reset();
    dragging_ = true;
    selection_changed();
    return true;
  }
  case pointer_action::move: {
    if (!dragging_) {
      return false;
    }
    point const local = p.position - content_origin();
    if (text::paragraph const *const para = paragraph_for(content_width())) {
      text::text_hit const hit = para->hit_test(local);
      caret_ = snap(from_display(hit.offset));
      affinity_ = hit.affinity;
    } else {
      caret_ = local.x <= 0 ? 0 : text_.size();
    }
    goal_x_.reset();
    selection_changed();
    return true;
  }
  case pointer_action::up:
    if (p.button != pointer_button::primary) {
      return false;
    }
    dragging_ = false;
    return true;
  case pointer_action::cancel:
    dragging_ = false;
    return true;
  default:
    return false;
  }
}

auto text_field::on_event(event_context &, event const &e) -> bool {
  if (auto const *p = std::get_if<pointer_event>(&e)) {
    return handle_pointer(*p);
  }
  if (auto const *k = std::get_if<key_event>(&e)) {
    return k->pressed && handle_key(*k);
  }
  if (auto const *t = std::get_if<text_input_event>(&e)) {
    if (read_only_) {
      return false;
    }
    insert_text(t->text);
    return true;
  }
  if (auto const *ime = std::get_if<ime_event>(&e)) {
    if (read_only_) {
      return false;
    }
    set_preedit(ime->phase == ime_phase::update ? ime->text : std::string{});
    return true;
  }
  return false;
}

auto text_field::on_interaction_changed(interaction_state const &previous) -> void {
  widget::on_interaction_changed(previous);
  if (previous.focused && !interaction().focused) {
    dragging_ = false;
    set_preedit({});
  }
}

auto text_field::accessibility_info() const -> accessibility::info {
  accessibility::info details;
  details.kind = accessibility::role::text_field;
  details.name = name_;
  details.value = text_;
  details.multiline = multiline_;
  details.read_only = read_only_;
  details.selection = accessibility::text_selection{anchor_, caret_};
  details.actions = {accessibility::action::focus, accessibility::action::set_text_selection};
  if (!read_only_) {
    details.actions.insert(accessibility::action::set_value);
  }
  return details;
}

auto text_field::on_accessibility_action(accessibility::action_request const &request) -> bool {
  switch (request.kind) {
  case accessibility::action::set_value:
    if (read_only_) {
      return false;
    }
    replace_range(0, text_.size(), multiline_ ? request.text : flatten_line_breaks(request.text));
    return true;
  case accessibility::action::set_text_selection:
    set_selection(request.selection.anchor, request.selection.focus);
    return true;
  default:
    return false;
  }
}

auto text_field::on_measure(constraints const &limits) const -> basalt::size {
  insets const pad = padding();
  auto const &sizes = theme().sizes;
  float const line = line_height();
  if (!multiline_) {
    return {resolve(preferred_width_),
            std::max(line + pad.vertical(), resolve(sizes.dimension_height_element_min))};
  }
  float const width = limits.bounded_width() ? limits.max.width : resolve(preferred_width_);
  float text_height = line;
  if (text::paragraph const *const p = paragraph_for(std::max(0.0f, width - pad.horizontal()))) {
    text_height = std::max(text_height, p->extent().height);
  }
  float const minimum = framed_ ? resolve(sizes.dimension_height_text_area_min) : 0;
  return {width, std::max(text_height + pad.vertical(), minimum)};
}

auto text_field::on_paint(render_context &context) const -> void {
  if (tree() == nullptr) {
    return;
  }
  basalt::theme const &t = theme();
  rect const area{{}, bounds().extent};
  if (framed_) {
    float const radius = resolve(t.sizes.radius_default);
    float const width = device_stroke_width(fixed(1), scale());
    context.fill_rounded_rect(area, radius, t.colors.background_default);
    context.stroke_rounded_rect(inset(area, insets::uniform(width / 2)), radius,
                                has_focus() ? t.colors.border_accent : t.colors.border_default,
                                width);
  }
  insets const pad = padding();
  auto const clip = context.clip(inset(area, pad));
  point const origin = content_origin();
  text::paragraph const *const p = paragraph_for(content_width());

  if (p != nullptr && has_selection() && preedit_.empty()) {
    for (rect const &r : p->selection_rects(selection_begin(), selection_end())) {
      context.fill_rect(translated(r, origin), t.colors.background_accent_subdued_active);
    }
  }
  if (text_.empty() && preedit_.empty()) {
    placeholder_.draw(context, *this, origin, t.colors.text_subdued);
  }
  if (p != nullptr) {
    for (auto const &line : p->lines()) {
      for (auto const &placed : line.runs) {
        context.draw_glyph_run({origin.x + placed.x, origin.y + line.y + line.baseline}, placed.run,
                               t.colors.text_default);
      }
    }
    if (!preedit_.empty()) {
      float const width = device_stroke_width(fixed(1), scale());
      for (rect const &r : p->selection_rects(caret_, caret_ + preedit_.size())) {
        rect const shown = translated(r, origin);
        context.line({shown.left(), shown.bottom() - width},
                     {shown.right(), shown.bottom() - width}, t.colors.text_default, width);
      }
    }
  }
  if (has_focus() && !read_only_) {
    rect const caret = caret_rect();
    float const width = device_stroke_width(fixed(1), scale());
    context.fill_rect({caret.origin, {width, caret.height()}}, t.colors.text_default);
  }
}

} // namespace basalt
