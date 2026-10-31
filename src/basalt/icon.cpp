#include "basalt/icon.h"

#include <algorithm>
#include <array>

namespace basalt {

namespace {

struct icon_entry {
  icon id;
  std::string_view name;
  char32_t codepoint;
};

constexpr auto icon_table = std::to_array<icon_entry>({
#include "icon_table.inc"
});

constexpr std::array<icon, icon_table.size()> icon_ids = [] {
  std::array<icon, icon_table.size()> ids{};
  std::ranges::transform(icon_table, ids.begin(), &icon_entry::id);
  return ids;
}();

constexpr unsigned char icon_font_bytes[] = {
#include "icon_font.inc"
};

auto entry_for(icon i) -> icon_entry const * {
  auto const found = std::ranges::find(icon_table, i, &icon_entry::id);
  return found == icon_table.end() ? nullptr : &*found;
}

} // namespace

auto icon_codepoint(icon i) -> char32_t {
  auto const *entry = entry_for(i);
  return entry == nullptr ? U'\0' : entry->codepoint;
}

auto icon_text(icon i) -> std::string {
  auto const codepoint = static_cast<unsigned>(icon_codepoint(i));
  if (codepoint == 0) {
    return {};
  }
  // Icons live in the private-use area below U+FFFF, which UTF-8 stores in three bytes.
  return {static_cast<char>(0xE0 | (codepoint >> 12)),
          static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)),
          static_cast<char>(0x80 | (codepoint & 0x3F))};
}

auto icon_from_name(std::string_view name) -> icon {
  auto const found = std::ranges::find(icon_table, name, &icon_entry::name);
  return found == icon_table.end() ? icon::none : found->id;
}

auto icon_name(icon i) -> std::string_view {
  auto const *entry = entry_for(i);
  return entry == nullptr ? std::string_view{} : entry->name;
}

auto all_icons() -> std::span<icon const> { return icon_ids; }

auto icon_font_data() -> std::shared_ptr<std::vector<std::byte> const> {
  static auto const data = [] {
    auto bytes = std::make_shared<std::vector<std::byte>>(std::size(icon_font_bytes));
    std::ranges::transform(icon_font_bytes, bytes->begin(),
                           [](unsigned char b) { return static_cast<std::byte>(b); });
    return std::shared_ptr<std::vector<std::byte> const>(std::move(bytes));
  }();
  return data;
}

} // namespace basalt
