#pragma once

#include "basalt/paint.h"

namespace basalt {

class render_context {
public:
  class scope {
  public:
    scope(scope const &) = delete;
    auto operator=(scope const &) -> scope & = delete;
    scope(scope &&other) noexcept;
    auto operator=(scope &&) -> scope & = delete;
    ~scope();

  private:
    friend class render_context;
    enum class kind { clip, transform, layer };

    scope(render_context &context, kind type) noexcept;
    render_context *context_;
    kind kind_;
  };

  explicit render_context(paint_commands &commands) noexcept;
  render_context(render_context const &) = delete;
  auto operator=(render_context const &) -> render_context & = delete;

  auto fill_rect(rect bounds, rgba color) -> void;
  auto stroke_rect(rect bounds, rgba color, float width) -> void;
  auto fill_rounded_rect(rect bounds, float radius, rgba color) -> void;
  auto stroke_rounded_rect(rect bounds, float radius, rgba color, float width) -> void;
  auto line(point from, point to, rgba color, float width) -> void;
  // `origin` is where the baseline of the run starts.
  auto draw_glyph_run(point origin, text::glyph_run_ptr run, rgba color) -> void;

  // Scales the whole image to fill `bounds`; a null image draws nothing.
  auto draw_image(rect bounds, raster_image_ptr image) -> void;

  // Clips and transforms are expressed in the coordinate space active when
  // they are pushed. Destroy the returned scope to restore the previous state.
  [[nodiscard]] auto clip(rect bounds) -> scope;
  [[nodiscard]] auto transform(affine_transform value) -> scope;
  [[nodiscard]] auto layer(std::uint8_t opacity = 255) -> scope;

private:
  auto end_scope(scope::kind type) -> void;
  paint_commands *commands_;
};

} // namespace basalt
