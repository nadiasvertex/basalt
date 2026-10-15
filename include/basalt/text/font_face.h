#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>

namespace basalt::text {

// Vertical metrics for one font size, in the same units as the size.
// `ascent` and `descent` are positive distances from the baseline.
struct font_metrics {
  float ascent = 0;
  float descent = 0;
  float line_gap = 0;

  [[nodiscard]] constexpr auto height() const -> float { return ascent + descent + line_gap; }
};

// One face of an OpenType font file, loaded and immutable. Faces are shared
// between the font manager, glyph runs and the renderer, which reads `data()`
// to build its own typeface.
class font_face {
public:
  font_face(font_face const &) = delete;
  auto operator=(font_face const &) -> font_face & = delete;
  ~font_face();

  // Unique within the process; renderers use it as a cache key.
  [[nodiscard]] auto id() const -> std::uint64_t;
  // Typographic family when the font has one, so weights group together.
  [[nodiscard]] auto family() const -> std::string const &;
  [[nodiscard]] auto style_name() const -> std::string const &;
  // CSS-style weight, 100 to 900.
  [[nodiscard]] auto weight() const -> int;
  // The weight-axis value this face sets on a variable font, or zero for a static font. Renderers
  // apply it when building a typeface from `data()`.
  [[nodiscard]] auto variation_weight() const -> int;
  [[nodiscard]] auto italic() const -> bool;
  // True when the digit-width and letter-width glyphs advance equally.
  [[nodiscard]] auto fixed_pitch() const -> bool;
  // Empty for faces created from memory.
  [[nodiscard]] auto path() const -> std::filesystem::path const &;
  // Index inside a font collection, zero for single-face files.
  [[nodiscard]] auto face_index() const -> unsigned;
  // The whole font file, which may hold more than this face.
  [[nodiscard]] auto data() const -> std::span<std::byte const>;
  [[nodiscard]] auto units_per_em() const -> unsigned;

  // The glyph id for `scalar`, or nothing if the font does not cover it.
  [[nodiscard]] auto glyph_for(char32_t scalar) const -> std::optional<std::uint32_t>;
  [[nodiscard]] auto covers(char32_t scalar) const -> bool { return glyph_for(scalar).has_value(); }
  [[nodiscard]] auto metrics(float size) const -> font_metrics;

  // Faces are made by `font_manager`; `impl` is only defined inside the library.
  struct impl;
  explicit font_face(std::unique_ptr<impl> state);

private:
  friend auto native_impl(font_face const &face) -> impl &;
  std::unique_ptr<impl> impl_;
};

using font_face_ptr = std::shared_ptr<font_face const>;

} // namespace basalt::text
