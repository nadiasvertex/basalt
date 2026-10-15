#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace basalt::text::detail {

struct decoded_scalar {
  char32_t value = 0;
  std::size_t length = 1;
};

inline constexpr auto replacement_character = char32_t{0xFFFD};

// Decodes the scalar at `pos`. Malformed input (stray continuation bytes,
// overlong forms, surrogates, values past U+10FFFF, truncation) yields U+FFFD
// and consumes one byte, so every byte belongs to exactly one scalar.
[[nodiscard]] constexpr auto decode_utf8(std::string_view text, std::size_t pos) -> decoded_scalar {
  auto const byte = [&](std::size_t i) { return static_cast<unsigned char>(text[i]); };
  auto const lead = byte(pos);
  if (lead < 0x80U) {
    return {lead, 1};
  }
  auto length = std::size_t{0};
  auto value = char32_t{0};
  auto minimum = char32_t{0};
  if (lead >= 0xC2U && lead <= 0xDFU) {
    length = 2;
    value = lead & 0x1FU;
    minimum = 0x80;
  } else if (lead >= 0xE0U && lead <= 0xEFU) {
    length = 3;
    value = lead & 0x0FU;
    minimum = 0x800;
  } else if (lead >= 0xF0U && lead <= 0xF4U) {
    length = 4;
    value = lead & 0x07U;
    minimum = 0x10000;
  } else {
    return {replacement_character, 1};
  }
  if (pos + length > text.size()) {
    return {replacement_character, 1};
  }
  for (auto i = std::size_t{1}; i < length; ++i) {
    auto const next = byte(pos + i);
    if ((next & 0xC0U) != 0x80U) {
      return {replacement_character, 1};
    }
    value = (value << 6) | (next & 0x3FU);
  }
  if (value < minimum || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF)) {
    return {replacement_character, 1};
  }
  return {value, length};
}

inline auto append_utf8(std::string &out, char32_t c) -> void {
  if (c < 0x80) {
    out += static_cast<char>(c);
  } else if (c < 0x800) {
    out += static_cast<char>(0xC0 | (c >> 6));
    out += static_cast<char>(0x80 | (c & 0x3F));
  } else if (c < 0x10000) {
    out += static_cast<char>(0xE0 | (c >> 12));
    out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (c & 0x3F));
  } else {
    out += static_cast<char>(0xF0 | (c >> 18));
    out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
    out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (c & 0x3F));
  }
}

} // namespace basalt::text::detail
