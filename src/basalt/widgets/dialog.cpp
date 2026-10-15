#include "basalt/widgets/dialog.h"

#include "basalt/widget_tree.h"
#include "basalt/widgets/label.h"

#include <utility>
#include <variant>

namespace basalt {

auto wrap_message(std::string const &text, std::size_t columns) -> std::vector<std::string> {
  std::vector<std::string> lines;
  std::size_t begin = 0;
  while (begin <= text.size()) {
    std::size_t end = text.find('\n', begin);
    if (end == std::string::npos) {
      end = text.size();
    }
    std::string current;
    std::size_t word = begin;
    while (word <= end) {
      std::size_t stop = text.find(' ', word);
      if (stop == std::string::npos || stop > end) {
        stop = end;
      }
      std::string const piece = text.substr(word, stop - word);
      if (!current.empty() && current.size() + 1 + piece.size() > columns) {
        lines.push_back(std::exchange(current, {}));
      }
      if (!current.empty()) {
        current += ' ';
      }
      current += piece;
      word = stop + 1;
    }
    lines.push_back(std::move(current));
    begin = end + 1;
  }
  return lines;
}

dialog::dialog(std::string title, std::string message, std::vector<dialog_button> buttons,
               std::size_t cancel_index)
    : box(axis::vertical), title_(std::move(title)), message_(std::move(message)),
      button_count_(buttons.size()), cancel_index_(cancel_index) {
  auto const &sizes = tokens::medium_sizes;
  set_padding(length_insets::symmetric(sizes.inset_horizontal_dialog, sizes.inset_vertical_dialog));
  set_gap(sizes.gap_vertical_dialog_text_buttons);

  auto &heading = emplace_child<label>(title_, text_role::heading_small);
  heading.set_heading_level(2);
  for (std::string const &line : wrap_message(message_, wrap_columns)) {
    emplace_child<label>(line);
  }
  auto &row = emplace_child<box>(axis::horizontal);
  row.set_gap(fixed(8));
  row.set_main_alignment(alignment::end);
  for (std::size_t i = 0; i < buttons.size(); ++i) {
    auto &b = row.emplace_child<button>(std::move(buttons[i].label));
    b.set_variant(buttons[i].variant);
    b.set_on_press([this, i] { choose(i); });
  }
}

auto dialog::choose(std::size_t index) -> bool {
  if (index >= button_count_) {
    return false;
  }
  if (on_choice_) {
    auto const handler = on_choice_;
    handler(index);
  }
  return true;
}

auto dialog::accessibility_info() const -> accessibility::info {
  accessibility::info details;
  details.kind = accessibility::role::dialog;
  details.name = title_;
  details.description = message_;
  return details;
}

auto dialog::on_event(event_context &, event const &e) -> bool {
  auto const *const k = std::get_if<key_event>(&e);
  if (k != nullptr && k->pressed && k->code == key::escape && cancel_index_ < button_count_) {
    return choose(cancel_index_);
  }
  return false;
}

auto dialog::on_paint(render_context &context) const -> void {
  if (tree() == nullptr) {
    return;
  }
  theme const &t = tree()->theme();
  rect const area{{}, bounds().extent};
  float const radius = resolve(t.sizes.radius_panel);
  float const width = device_stroke_width(fixed(1), scale());
  context.fill_rounded_rect(area, radius, t.colors.background_dialog);
  context.stroke_rounded_rect(inset(area, insets::uniform(width / 2)), radius,
                              t.colors.border_default, width);
}

} // namespace basalt
