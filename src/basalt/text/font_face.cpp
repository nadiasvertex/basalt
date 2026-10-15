#include "font_face_impl.h"

namespace basalt::text {

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

} // namespace basalt::text
