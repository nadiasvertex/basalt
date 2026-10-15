#include "basalt/render_context.h"

#include <utility>

namespace basalt {

render_context::scope::scope(render_context &context, kind type) noexcept
    : context_(&context), kind_(type) {}

render_context::scope::scope(scope &&other) noexcept
    : context_(std::exchange(other.context_, nullptr)), kind_(other.kind_) {}

render_context::scope::~scope() {
  if (context_ != nullptr) {
    context_->end_scope(kind_);
  }
}

render_context::render_context(paint_commands &commands) noexcept : commands_(&commands) {}

auto render_context::fill_rect(rect bounds, rgba color) -> void {
  commands_->emplace_back(fill_rect_command{bounds, color});
}

auto render_context::stroke_rect(rect bounds, rgba color, float width) -> void {
  commands_->emplace_back(stroke_rect_command{bounds, color, width});
}

auto render_context::fill_rounded_rect(rect bounds, float radius, rgba color) -> void {
  commands_->emplace_back(fill_rounded_rect_command{bounds, color, radius});
}

auto render_context::stroke_rounded_rect(rect bounds, float radius, rgba color, float width)
    -> void {
  commands_->emplace_back(stroke_rounded_rect_command{bounds, color, radius, width});
}

auto render_context::line(point from, point to, rgba color, float width) -> void {
  commands_->emplace_back(line_command{from, to, color, width});
}

auto render_context::draw_glyph_run(point origin, text::glyph_run_ptr run, rgba color) -> void {
  commands_->emplace_back(draw_glyph_run_command{origin, std::move(run), color});
}

auto render_context::draw_image(rect bounds, raster_image_ptr image) -> void {
  if (image == nullptr) {
    return;
  }
  commands_->emplace_back(draw_image_command{bounds, std::move(image)});
}

auto render_context::clip(rect bounds) -> scope {
  commands_->emplace_back(push_clip_command{bounds});
  return scope{*this, scope::kind::clip};
}

auto render_context::transform(affine_transform value) -> scope {
  commands_->emplace_back(push_transform_command{value});
  return scope{*this, scope::kind::transform};
}

auto render_context::layer(std::uint8_t opacity) -> scope {
  commands_->emplace_back(begin_layer_command{opacity});
  return scope{*this, scope::kind::layer};
}

auto render_context::end_scope(scope::kind type) -> void {
  switch (type) {
  case scope::kind::clip:
    commands_->emplace_back(pop_clip_command{});
    break;
  case scope::kind::transform:
    commands_->emplace_back(pop_transform_command{});
    break;
  case scope::kind::layer:
    commands_->emplace_back(end_layer_command{});
    break;
  }
}

} // namespace basalt
