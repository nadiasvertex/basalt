#include "basalt/theme.h"

namespace basalt {

auto colors_for(color_scheme scheme) -> tokens::color_set const & {
  switch (scheme) {
  case color_scheme::dark:
    return tokens::dark_colors;
  case color_scheme::light:
    break;
  }
  return tokens::light_colors;
}

auto sizes_for(basalt::density tier) -> tokens::size_set const & {
  switch (tier) {
  case density::small:
    return tokens::small_sizes;
  case density::large:
    return tokens::large_sizes;
  case density::medium:
    break;
  }
  return tokens::medium_sizes;
}

auto make_theme(color_scheme scheme, basalt::density tier) -> theme {
  theme result;
  result.scheme = scheme;
  result.density = tier;
  result.colors = colors_for(scheme);
  result.sizes = sizes_for(tier);
  return result;
}

} // namespace basalt
