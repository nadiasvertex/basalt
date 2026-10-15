#include "basalt/widgets/avatar.h"

#include "basalt/widget_tree.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace basalt {

namespace a11y = accessibility;

namespace {

// The length of the UTF-8 sequence that starts with `lead`.
auto sequence_length(unsigned char lead) -> std::size_t {
  if (lead >= 0xF0) {
    return 4;
  }
  if (lead >= 0xE0) {
    return 3;
  }
  if (lead >= 0xC0) {
    return 2;
  }
  return 1;
}

auto first_letter(std::string_view word) -> std::string {
  if (word.empty()) {
    return {};
  }
  auto const length =
      std::min(sequence_length(static_cast<unsigned char>(word.front())), word.size());
  std::string letter{word.substr(0, length)};
  if (length == 1 && letter.front() >= 'a' && letter.front() <= 'z') {
    letter.front() = static_cast<char>(letter.front() - 'a' + 'A');
  }
  return letter;
}

auto text_role_for_name(control_size size) -> text_role {
  return size == control_size::small ? text_role::small_strong : text_role::body_strong;
}

} // namespace

auto avatar_initials(std::string_view name) -> std::string {
  std::vector<std::string_view> words;
  std::size_t start = std::string_view::npos;
  for (std::size_t i = 0; i <= name.size(); ++i) {
    bool const space = i == name.size() || name[i] == ' ' || name[i] == '\t';
    if (!space && start == std::string_view::npos) {
      start = i;
    } else if (space && start != std::string_view::npos) {
      words.push_back(name.substr(start, i - start));
      start = std::string_view::npos;
    }
  }
  if (words.empty()) {
    return {};
  }
  std::string result = first_letter(words.front());
  if (words.size() > 1) {
    result += first_letter(words.back());
  }
  return result;
}

avatar::avatar(std::string name, control_size size)
    : name_(std::move(name), text_role_for_name(size)), supporting_({}, text_role::small),
      initials_({}, label_role_for(size)), size_(size) {
  initials_.set_text(avatar_initials(name_.text()));
}

auto avatar::diameter() const -> float {
  float const base = resolve(theme().sizes.dimension_avatar);
  switch (size_) {
  case control_size::small:
    return base;
  case control_size::large:
    return base * 5 / 3;
  case control_size::medium:
    break;
  }
  return base * 4 / 3;
}

auto avatar::refresh_tooltip() -> void {
  tip_.set_text(*this, layout_ == avatar_layout::graphic_only ? name_.text() : std::string{});
}

auto avatar::set_name(std::string name) -> void {
  if (name_.set_text(std::move(name))) {
    initials_.set_text(avatar_initials(name_.text()));
    refresh_tooltip();
    invalidate_content();
  }
}

auto avatar::set_supporting_text(std::string text) -> void {
  if (supporting_.set_text(std::move(text))) {
    invalidate_content();
  }
}

auto avatar::set_image(raster_image_ptr image) -> void {
  image_ = std::move(image);
  invalidate_paint();
}

auto avatar::set_layout(avatar_layout layout) -> void {
  if (layout != layout_) {
    layout_ = layout;
    refresh_tooltip();
    invalidate_content();
  }
}

auto avatar::set_size(control_size size) -> void {
  if (size != size_) {
    size_ = size;
    name_.set_role(text_role_for_name(size));
    initials_.set_role(label_role_for(size));
    invalidate_content();
  }
}

auto avatar::accessibility_info() const -> a11y::info {
  a11y::info details;
  if (layout_ == avatar_layout::graphic_only) {
    details.kind = a11y::role::image;
    details.name = name_.text();
    details.description = supporting_.text();
    return details;
  }
  details.kind = a11y::role::group;
  details.name = name_.text();
  details.description = supporting_.text();
  return details;
}

auto avatar::on_measure(constraints const &) const -> basalt::size {
  float const d = diameter();
  if (layout_ == avatar_layout::graphic_only) {
    return {d, d};
  }
  float const gap = resolve(theme().sizes.gap_horizontal_shape_text);
  basalt::size const name = name_.measure(*this);
  basalt::size const support =
      supporting_.text().empty() ? basalt::size{} : supporting_.measure(*this);
  float const text_width = std::max(name.width, support.width);
  return {d + (text_width > 0 ? gap + text_width : 0), std::max(d, name.height + support.height)};
}

auto avatar::on_paint(render_context &context) const -> void {
  if (tree() == nullptr) {
    return;
  }
  basalt::theme const &t = theme();
  float const d = diameter();
  float const gap = resolve(t.sizes.gap_horizontal_shape_text);
  basalt::size const own = bounds().extent;
  basalt::size const name = name_.measure(*this);
  basalt::size const support =
      supporting_.text().empty() ? basalt::size{} : supporting_.measure(*this);
  float const text_width = std::max(name.width, support.width);
  bool const leading = layout_ == avatar_layout::leading;
  float const graphic_left = leading && layout_ != avatar_layout::graphic_only
                                 ? text_width + (text_width > 0 ? gap : 0)
                                 : 0;
  rect const graphic{{graphic_left, (own.height - d) / 2}, {d, d}};

  if (image_ != nullptr) {
    context.draw_image(graphic, image_);
  } else {
    context.fill_rounded_rect(graphic, d / 2, t.colors.background_accent);
    initials_.draw_in(context, *this, graphic, t.colors.text_default_on_dark, alignment::center,
                      alignment::center);
  }
  if (layout_ == avatar_layout::graphic_only) {
    return;
  }
  float const text_left = leading ? 0 : d + gap;
  float const block = name.height + support.height;
  float const top = (own.height - block) / 2;
  name_.draw(context, *this, {text_left, top}, t.colors.text_default);
  if (support.height > 0) {
    supporting_.draw(context, *this, {text_left, top + name.height}, t.colors.text_subdued);
  }
}

auto avatar::on_interaction_changed(interaction_state const &previous) -> void {
  widget::on_interaction_changed(previous);
  if (tip_.update(*this)) {
    request_animation_frame();
  }
}

auto avatar::on_animation_frame(std::chrono::nanoseconds elapsed) -> bool {
  return tip_.advance(*this, elapsed);
}

auto avatar::on_detached() -> void { tip_.hide(*this); }

} // namespace basalt
