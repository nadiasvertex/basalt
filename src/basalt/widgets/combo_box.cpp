#include "basalt/widgets/combo_box.h"

#include "basalt/event_router.h"
#include "basalt/widget_tree.h"
#include "basalt/widgets/overlay_host.h"
#include "basalt/widgets/text_field.h"

#include <algorithm>
#include <cctype>
#include <string_view>
#include <utility>
#include <variant>

#include "glyphs.h"
#include "notify.h"

namespace basalt {

namespace a11y = accessibility;

namespace {

// The list scrolls beyond this many rows.
constexpr std::size_t visible_rows = 8;

auto lowered(std::string_view text) -> std::string {
  std::string out(text);
  std::ranges::transform(out, out.begin(),
                         [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

// One line of the list: a section heading, or a suggestion with its index among the matches.
struct list_row {
  std::string text;
  std::optional<std::size_t> item;
};

// The panel shown in the popup. It is never focused: the text field keeps focus, and the owner
// moves the highlight.
class suggestion_list : public widget {
public:
  explicit suggestion_list(float min_width) : min_width_(min_width) {}

  auto set_rows(std::vector<list_row> rows) -> void {
    rows_ = std::move(rows);
    lines_.clear();
    for (list_row const &row : rows_) {
      lines_.emplace_back(row.text, row.item ? text_role::body : text_role::small);
    }
    first_ = std::min(first_, hidden_rows());
    invalidate_content();
  }

  auto set_highlight(std::optional<std::size_t> item) -> void {
    highlighted_ = item;
    if (item) {
      auto const found = std::ranges::find(rows_, item, &list_row::item);
      if (found != rows_.end()) {
        reveal(static_cast<std::size_t>(found - rows_.begin()));
      }
    }
    invalidate_paint();
  }

  auto set_on_hover(std::function<void(std::size_t)> handler) -> void {
    on_hover_ = std::move(handler);
  }
  auto set_on_pick(std::function<void(std::size_t)> handler) -> void {
    on_pick_ = std::move(handler);
  }

  [[nodiscard]] auto accessibility_info() const -> a11y::info override {
    a11y::info details;
    details.kind = a11y::role::list_box;
    return details;
  }

protected:
  [[nodiscard]] auto on_measure(constraints const &) const -> basalt::size override {
    float widest = 0;
    for (text_line const &line : lines_) {
      widest = std::max(widest, line.measure(*this).width);
    }
    float const width = widest + 2 * resolve(theme().sizes.inset_horizontal_menu_item);
    return {std::max(width, min_width_), row_height() * static_cast<float>(shown_rows()) + 8};
  }

  auto on_event(event_context &, event const &e) -> bool override {
    if (auto const *p = std::get_if<pointer_event>(&e)) {
      if (p->action == pointer_action::move || p->action == pointer_action::enter) {
        if (auto const item = item_at(p->position)) {
          notify(on_hover_, *item);
        }
        return true;
      }
      if (p->button != pointer_button::primary) {
        return false;
      }
      if (p->action == pointer_action::up) {
        if (auto const item = item_at(p->position)) {
          notify(on_pick_, *item);
        }
      }
      return p->action == pointer_action::down || p->action == pointer_action::up;
    }
    if (auto const *w = std::get_if<wheel_event>(&e)) {
      float const step = w->precise ? w->delta.y / row_height() : w->delta.y;
      scroll_by(static_cast<int>(step));
      return true;
    }
    return false;
  }

  auto on_paint(render_context &context) const -> void override {
    if (tree() == nullptr) {
      return;
    }
    basalt::theme const &t = theme();
    rect const area{{}, bounds().extent};
    float const radius = resolve(t.sizes.radius_default);
    float const stroke = device_stroke_width(fixed(1), scale());
    context.fill_rounded_rect(area, radius, t.colors.background_default);
    context.stroke_rounded_rect(inset(area, insets::uniform(stroke / 2)), radius,
                                t.colors.border_default, stroke);
    float const inset_x = resolve(t.sizes.inset_horizontal_menu_item);
    float const height = row_height();
    for (std::size_t i = 0; i < shown_rows(); ++i) {
      std::size_t const index = first_ + i;
      rect const row{{0, 4 + height * static_cast<float>(i)}, {area.width(), height}};
      bool const is_item = rows_[index].item.has_value();
      if (is_item && rows_[index].item == highlighted_) {
        context.fill_rect(row, t.colors.background_accent_transparent_hover);
      }
      rect const text_area{{row.left() + inset_x, row.top()}, {row.width() - 2 * inset_x, height}};
      lines_[index].draw_in(context, *this, text_area,
                            is_item ? t.colors.text_default : t.colors.text_subdued);
    }
  }

private:
  [[nodiscard]] auto row_height() const -> float {
    return resolve(theme().sizes.dimension_height_element_min);
  }
  [[nodiscard]] auto shown_rows() const -> std::size_t {
    return std::min(rows_.size(), visible_rows);
  }
  // The largest value of `first_` that still fills the window.
  [[nodiscard]] auto hidden_rows() const -> std::size_t { return rows_.size() - shown_rows(); }

  [[nodiscard]] auto item_at(point position) const -> std::optional<std::size_t> {
    float const y = position.y - 4;
    if (y < 0) {
      return std::nullopt;
    }
    std::size_t const index = first_ + static_cast<std::size_t>(y / row_height());
    if (index >= rows_.size() || index >= first_ + shown_rows()) {
      return std::nullopt;
    }
    return rows_[index].item;
  }

  auto reveal(std::size_t row) -> void {
    if (row < first_) {
      first_ = row;
    } else if (row >= first_ + shown_rows()) {
      first_ = row + 1 - shown_rows();
    }
    // A heading above the first suggestion of a section stays visible with it.
    if (first_ == 1 && !rows_.front().item) {
      first_ = 0;
    }
  }

  auto scroll_by(int rows) -> void {
    auto const target =
        std::clamp<long>(static_cast<long>(first_) + rows, 0, static_cast<long>(hidden_rows()));
    if (static_cast<std::size_t>(target) != first_) {
      first_ = static_cast<std::size_t>(target);
      invalidate_paint();
    }
  }

  float min_width_;
  std::vector<list_row> rows_;
  std::vector<text_line> lines_;
  std::size_t first_ = 0;
  std::optional<std::size_t> highlighted_;
  std::function<void(std::size_t)> on_hover_;
  std::function<void(std::size_t)> on_pick_;
};

} // namespace

struct combo_box::layout_parts {
  basalt::size label;
  basalt::size helper;
  float field_height = 0;
  float field_width = 0;
  float inset = 0;
  float mark = 0;
  float label_gap = 0;
  float helper_gap = 0;
  float field_top = 0;
};

combo_box::combo_box(std::string label, control_size size) : field_(std::move(label), size) {
  input_ = &emplace_child<text_field>();
  input_->set_framed(false);
  input_->set_preferred_width(fixed(160));
  input_->set_accessible_name(field_.label().text());
  input_->set_placeholder(field_.placeholder().text());
  input_->set_on_change([this](std::string const &) { typed(); });
  input_->set_on_submit(
      [this](std::string const &text) { submit(highlighted_suggestion().value_or(text)); });
}

combo_box::~combo_box() = default;

auto combo_box::parts() const -> layout_parts {
  basalt::theme const &t = theme();
  layout_parts p;
  p.label = field_.label().text().empty() ? basalt::size{} : field_.label().measure(*this);
  p.helper = field_.helper().text().empty() ? basalt::size{} : field_.helper().measure(*this);
  p.inset = resolve(t.sizes.inset_horizontal_input);
  p.mark = resolve(t.sizes.dimension_icon);
  p.label_gap = p.label.height > 0 ? resolve(t.sizes.gap_vertical_field_input) : 0;
  p.helper_gap = p.helper.height > 0 ? resolve(t.sizes.gap_vertical_field_helper_text) : 0;
  p.field_top = p.label.height + p.label_gap;
  basalt::size const input = input_->measure(constraints::loose({unbounded, unbounded}));
  p.field_height = std::max(input.height, resolve(t.sizes.dimension_height_element_min));
  p.field_width = std::max(p.inset + input.width + p.inset + p.mark + p.inset,
                           resolve(t.sizes.dimension_width_popover_min));
  return p;
}

auto combo_box::set_label(std::string label) -> void {
  if (field_.set_label(label)) {
    input_->set_accessible_name(std::move(label));
    invalidate_content();
  }
}

auto combo_box::set_helper_text(std::string text) -> void {
  if (field_.set_helper(std::move(text))) {
    invalidate_content();
  }
}

auto combo_box::set_placeholder(std::string text) -> void {
  if (field_.set_placeholder(text)) {
    input_->set_placeholder(std::move(text));
    invalidate_content();
  }
}

auto combo_box::set_error(bool error) -> void {
  if (field_.set_error(error)) {
    invalidate_paint();
  }
}

auto combo_box::set_size(control_size size) -> void {
  if (field_.set_size(size)) {
    invalidate_content();
  }
}

auto combo_box::text() const -> std::string const & { return input_->text(); }

auto combo_box::set_text(std::string text) -> void {
  input_->set_text(std::move(text));
  highlighted_.reset();
  refresh_list();
}

auto combo_box::add_suggestion(std::string section, std::string text) -> void {
  suggestions_.push_back({std::move(section), std::move(text)});
}

auto combo_box::clear_suggestions() -> void {
  suggestions_.clear();
  highlighted_.reset();
  dismiss_popup();
}

auto combo_box::matches() const -> std::vector<suggestion> {
  std::string const needle = lowered(text());
  std::vector<std::string_view> sections;
  for (suggestion const &entry : suggestions_) {
    if (std::ranges::find(sections, entry.section) == sections.end()) {
      sections.push_back(entry.section);
    }
  }
  std::vector<suggestion> found;
  for (std::string_view const section : sections) {
    for (suggestion const &entry : suggestions_) {
      bool const listed = std::ranges::any_of(
          found, [&](suggestion const &other) { return other.text == entry.text; });
      if (entry.section == section && !listed && lowered(entry.text).contains(needle)) {
        found.push_back(entry);
      }
    }
  }
  return found;
}

auto combo_box::matching_suggestions() const -> std::vector<std::string> {
  std::vector<std::string> names;
  for (suggestion const &entry : matches()) {
    names.push_back(entry.text);
  }
  return names;
}

auto combo_box::highlighted_suggestion() const -> std::optional<std::string> {
  if (!highlighted_ || !is_open()) {
    return std::nullopt;
  }
  auto const names = matching_suggestions();
  return *highlighted_ < names.size() ? std::optional{names[*highlighted_]} : std::nullopt;
}

auto combo_box::is_open() const -> bool {
  overlay_host const *const host = overlay_host::find(*this);
  return host != nullptr && popup_.valid() && list_ != nullptr;
}

auto combo_box::open() -> bool {
  if (!enabled_in_tree() || is_open()) {
    return false;
  }
  overlay_host *const host = overlay_host::find(*this);
  if (host == nullptr || matches().empty()) {
    return false;
  }
  layout_parts const p = parts();
  auto shown = std::make_unique<suggestion_list>(bounds().width());
  list_ = shown.get();
  auto *const list = static_cast<suggestion_list *>(list_);
  list->set_on_hover([this](std::size_t item) {
    highlighted_ = item;
    static_cast<suggestion_list *>(list_)->set_highlight(item);
  });
  list->set_on_pick([this](std::size_t item) {
    auto const names = matching_suggestions();
    if (item < names.size()) {
      choose(names[item]);
    }
  });

  rect const anchor = bounds_in_root();
  rect const placed = {host->from_root({anchor.origin.x, anchor.origin.y + p.field_top}),
                       {bounds().width(), p.field_height}};
  popup_ = host->show_hover_popup(std::move(shown), placed, [this] {
    popup_ = {};
    list_ = nullptr;
  });
  refresh_list();
  invalidate_paint();
  return true;
}

auto combo_box::dismiss_popup() -> void {
  if (overlay_host *const host = overlay_host::find(*this); host != nullptr && popup_.valid()) {
    host->dismiss(popup_);
  }
  popup_ = {};
  list_ = nullptr;
  highlighted_.reset();
}

auto combo_box::close() -> void {
  dismiss_popup();
  invalidate_paint();
}

auto combo_box::refresh_list() -> void {
  if (list_ == nullptr) {
    return;
  }
  auto const found = matches();
  if (found.empty()) {
    close();
    return;
  }
  std::vector<list_row> rows;
  std::string section;
  for (std::size_t item = 0; item < found.size(); ++item) {
    bool const heading =
        !found[item].section.empty() && (item == 0 || found[item].section != section);
    if (heading) {
      rows.push_back({found[item].section, std::nullopt});
    }
    section = found[item].section;
    rows.push_back({found[item].text, item});
  }
  if (highlighted_ && *highlighted_ >= found.size()) {
    highlighted_.reset();
  }
  auto *const list = static_cast<suggestion_list *>(list_);
  list->set_rows(std::move(rows));
  list->set_highlight(highlighted_);
}

auto combo_box::typed() -> void {
  highlighted_.reset();
  if (is_open()) {
    refresh_list();
  } else {
    open();
  }
  notify(on_change_, text());
}

auto combo_box::submit(std::string text) -> void {
  input_->set_text(text);
  close();
  notify(on_submit_, text);
}

auto combo_box::choose(std::string text) -> void { submit(std::move(text)); }

auto combo_box::move_highlight(int direction) -> void {
  std::size_t const count = matching_suggestions().size();
  if (count == 0) {
    return;
  }
  if (!highlighted_) {
    highlighted_ = direction > 0 ? 0 : count - 1;
  } else if (direction > 0) {
    highlighted_ = std::min(*highlighted_ + 1, count - 1);
  } else if (*highlighted_ == 0) {
    highlighted_.reset();
  } else {
    highlighted_ = *highlighted_ - 1;
  }
  static_cast<suggestion_list *>(list_)->set_highlight(highlighted_);
}

auto combo_box::on_detached() -> void { dismiss_popup(); }

auto combo_box::on_interaction_changed(interaction_state const &previous) -> void {
  widget::on_interaction_changed(previous);
  if (previous.focus_within && !interaction().focus_within) {
    close();
  }
}

auto combo_box::accessibility_info() const -> a11y::info {
  a11y::info details;
  details.kind = a11y::role::combo_box;
  details.name = field_.label().text();
  details.description = field_.helper().text();
  details.value = text();
  details.invalid = field_.has_error();
  details.expanded = is_open();
  details.actions = {a11y::action::press};
  return details;
}

auto combo_box::on_accessibility_action(a11y::action_request const &request) -> bool {
  if (request.kind != a11y::action::press) {
    return false;
  }
  if (is_open()) {
    close();
    return true;
  }
  return open();
}

auto combo_box::on_event(event_context &, event const &e) -> bool {
  if (auto const *p = std::get_if<pointer_event>(&e)) {
    if (p->button != pointer_button::primary) {
      return false;
    }
    if (p->action == pointer_action::down) {
      return true;
    }
    if (p->action == pointer_action::up) {
      if (interaction().pressed) {
        (void)input_->request_focus();
        if (!open()) {
          close();
        }
      }
      return true;
    }
    return false;
  }
  if (auto const *k = std::get_if<key_event>(&e)) {
    if (!k->pressed || k->modifiers != modifier::none) {
      return false;
    }
    if (k->code == key::escape) {
      bool const was_open = is_open();
      close();
      return was_open;
    }
    if (k->code == key::down || k->code == key::up) {
      if (!is_open()) {
        return k->code == key::down && open();
      }
      move_highlight(k->code == key::down ? 1 : -1);
      return true;
    }
  }
  return false;
}

auto combo_box::on_measure(constraints const &) const -> basalt::size {
  layout_parts const p = parts();
  float const width = std::max({p.field_width, p.label.width, p.helper.width});
  return {width, p.field_top + p.field_height + p.helper_gap + p.helper.height};
}

auto combo_box::on_layout() -> void {
  layout_parts const p = parts();
  basalt::size const wanted = input_->measure(constraints::loose({unbounded, unbounded}));
  float const width = std::max(0.0f, bounds().width() - 3 * p.inset - p.mark);
  input_->set_bounds(
      {{p.inset, p.field_top + (p.field_height - wanted.height) / 2}, {width, wanted.height}});
}

auto combo_box::on_paint(render_context &context) const -> void {
  if (tree() == nullptr) {
    return;
  }
  basalt::theme const &t = theme();
  layout_parts const p = parts();
  float const width = bounds().width();
  if (p.label.height > 0) {
    field_.label().draw(context, *this, {0, 0}, t.colors.text_default);
  }

  rect const field{{0, p.field_top}, {width, p.field_height}};
  float const radius = resolve(t.sizes.radius_default);
  float const stroke = device_stroke_width(fixed(1), scale());
  control_state const state = control_state_of(interaction(), enabled());
  context.fill_rounded_rect(field, radius,
                            state == control_state::hovered
                                ? t.colors.background_accent_transparent_hover
                                : t.colors.background_default);
  rgba const border = field_.has_error()                          ? t.colors.border_danger
                      : (interaction().focus_within || is_open()) ? t.colors.border_accent
                                                                  : t.colors.border_default;
  context.stroke_rounded_rect(inset(field, insets::uniform(stroke / 2)), radius, border, stroke);

  float const mark_width = device_stroke_width(fixed(1.5f), scale());
  float const mark_top = field.top() + (field.height() - p.mark) / 2;
  glyphs::draw_chevron(context, {{width - p.inset - p.mark, mark_top}, {p.mark, p.mark}},
                       !is_open(), t.colors.icon_default, mark_width);

  if (p.helper.height > 0) {
    field_.helper().draw(context, *this, {0, field.bottom() + p.helper_gap},
                         field_.has_error() ? t.colors.text_danger : t.colors.text_subdued);
  }
}

} // namespace basalt
