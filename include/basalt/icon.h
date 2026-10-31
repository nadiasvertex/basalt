#pragma once

#include "basalt/icon_ids.h"

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace basalt {

// The family name of the bundled icon font. Icons are drawn with this family only, so other text
// never falls back to it.
inline constexpr std::string_view icon_font_family = "Phosphor";

// The private-use codepoint of the icon in the bundled font, or 0 for `icon::none`.
[[nodiscard]] auto icon_codepoint(icon i) -> char32_t;
// The codepoint of the icon as UTF-8, ready to shape in the icon font. Empty for `icon::none`.
[[nodiscard]] auto icon_text(icon i) -> std::string;
// "bold" gives `icon::bold`. A name that no icon has gives `icon::none`.
[[nodiscard]] auto icon_from_name(std::string_view name) -> icon;
// The name of the icon, or an empty string for `icon::none`.
[[nodiscard]] auto icon_name(icon i) -> std::string_view;
// Every icon except `icon::none`, in declaration order.
[[nodiscard]] auto all_icons() -> std::span<icon const>;
// The bytes of the bundled icon font, a subset of Phosphor (MIT license, in third_party/phosphor).
// Add them to a `text::font_manager` with `add_data`.
[[nodiscard]] auto icon_font_data() -> std::shared_ptr<std::vector<std::byte> const>;

} // namespace basalt
