#include <array>
#include <initializer_list>
#include <utility>

#include "font_face_impl.h"

namespace basalt::text {
namespace {

struct outline_builder {
  glyph_outline outline;
  float scale = 1;

  auto add(outline_verb verb, std::initializer_list<std::array<float, 2>> points) -> void {
    outline.verbs.push_back(verb);
    for (auto const &[x, y] : points) {
      // Font units grow upward; Basalt's y grows downward.
      outline.points.push_back({x * scale, -y * scale});
    }
  }
};

auto builder_of(void *data) -> outline_builder & { return *static_cast<outline_builder *>(data); }

auto outline_funcs() -> hb_draw_funcs_t * {
  static auto *const funcs = [] {
    auto *const created = hb_draw_funcs_create();
    hb_draw_funcs_set_move_to_func(
        created,
        [](hb_draw_funcs_t *, void *data, hb_draw_state_t *, float x, float y, void *) {
          builder_of(data).add(outline_verb::move_to, {{x, y}});
        },
        nullptr, nullptr);
    hb_draw_funcs_set_line_to_func(
        created,
        [](hb_draw_funcs_t *, void *data, hb_draw_state_t *, float x, float y, void *) {
          builder_of(data).add(outline_verb::line_to, {{x, y}});
        },
        nullptr, nullptr);
    hb_draw_funcs_set_quadratic_to_func(
        created,
        [](hb_draw_funcs_t *, void *data, hb_draw_state_t *, float control_x, float control_y,
           float x, float y, void *) {
          builder_of(data).add(outline_verb::quad_to, {{control_x, control_y}, {x, y}});
        },
        nullptr, nullptr);
    hb_draw_funcs_set_cubic_to_func(
        created,
        [](hb_draw_funcs_t *, void *data, hb_draw_state_t *, float control1_x, float control1_y,
           float control2_x, float control2_y, float x, float y, void *) {
          builder_of(data).add(outline_verb::cubic_to,
                               {{control1_x, control1_y}, {control2_x, control2_y}, {x, y}});
        },
        nullptr, nullptr);
    hb_draw_funcs_set_close_path_func(
        created,
        [](hb_draw_funcs_t *, void *data, hb_draw_state_t *, void *) {
          builder_of(data).add(outline_verb::close, {});
        },
        nullptr, nullptr);
    hb_draw_funcs_make_immutable(created);
    return created;
  }();
  return funcs;
}

} // namespace

font_face::font_face(std::unique_ptr<impl> state) : impl_(std::move(state)) {}
font_face::~font_face() = default;

auto native_impl(font_face const &face) -> font_face::impl & { return *face.impl_; }

auto font_face::id() const -> std::uint64_t { return impl_->id; }
auto font_face::family() const -> std::string const & { return impl_->family; }
auto font_face::style_name() const -> std::string const & { return impl_->style; }
auto font_face::weight() const -> int { return impl_->weight; }
auto font_face::variation_weight() const -> int { return impl_->variation_weight; }
auto font_face::italic() const -> bool { return impl_->italic; }
auto font_face::fixed_pitch() const -> bool { return impl_->fixed_pitch; }
auto font_face::path() const -> std::filesystem::path const & { return impl_->path; }
auto font_face::face_index() const -> unsigned { return impl_->index; }
auto font_face::units_per_em() const -> unsigned { return impl_->upem; }

auto font_face::data() const -> std::span<std::byte const> {
  auto length = 0U;
  auto const *bytes = hb_blob_get_data(impl_->blob.get(), &length);
  return {reinterpret_cast<std::byte const *>(bytes), length};
}

auto font_face::glyph_for(char32_t scalar) const -> std::optional<std::uint32_t> {
  auto glyph = hb_codepoint_t{0};
  if (hb_font_get_nominal_glyph(impl_->lookup_font.get(), scalar, &glyph) != 0) {
    return glyph;
  }
  return std::nullopt;
}

auto font_face::metrics(float size) const -> font_metrics {
  auto extents = hb_font_extents_t{};
  if (hb_font_get_h_extents(impl_->lookup_font.get(), &extents) == 0) {
    return {};
  }
  auto const scale = size / static_cast<float>(impl_->upem);
  return {static_cast<float>(extents.ascender) * scale,
          static_cast<float>(-extents.descender) * scale,
          static_cast<float>(extents.line_gap) * scale};
}

auto font_face::outline(std::uint32_t glyph, float size) const -> glyph_outline {
  // The lookup font is unscaled, so HarfBuzz reports font units.
  auto builder = outline_builder{.outline = {}, .scale = size / static_cast<float>(impl_->upem)};
  hb_font_draw_glyph(impl_->lookup_font.get(), glyph, outline_funcs(), &builder);
  return std::move(builder.outline);
}

} // namespace basalt::text
