#pragma once

#include "basalt/tokens.h"
#include "basalt/typography.h"

#include <cstdint>

namespace basalt {

enum class color_scheme : std::uint8_t { light, dark };

// Cubit density tiers; medium is the default for application UI.
enum class density : std::uint8_t { small, medium, large };

// Resolved Cubit token values for one color scheme and density; a
// value-initialized theme is light at medium density. Widgets read
// semantic tokens from here instead of primitives so that switching scheme or
// density restyles the whole tree. Values are in token pixels; convert with
// `to_layout` and the window's `scale_factors`.
struct theme {
  color_scheme scheme = color_scheme::light;
  basalt::density density = density::medium;
  tokens::color_set colors = tokens::light_colors;
  tokens::size_set sizes = tokens::medium_sizes;
  tokens::text_style_set text = tokens::text_styles;
  // Long-form article content: body size with the loose line height.
  text_style reading_text = with_line_height(tokens::text::body_regular_large,
                                             tokens::primitive::type_line_height_loose_base);
  // Archive and XML source text in the monospace family.
  text_style source_text = with_family(tokens::text::body_regular_large, monospace_family);
  // Applied to the whole control, in [0, 1].
  float disabled_opacity = opacity_from_percent(tokens::primitive::opacity_disabled);

  [[nodiscard]] friend auto operator==(theme const &, theme const &) -> bool = default;
};

[[nodiscard]] auto colors_for(color_scheme scheme) -> tokens::color_set const &;
[[nodiscard]] auto sizes_for(basalt::density tier) -> tokens::size_set const &;

[[nodiscard]] auto make_theme(color_scheme scheme, basalt::density tier = density::medium) -> theme;

} // namespace basalt
