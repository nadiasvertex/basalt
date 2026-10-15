#include "basalt/text/line_break.h"

#include <hb.h>

#include "utf8.h"

namespace basalt::text {
namespace {

auto is_cjk(char32_t c) -> bool {
  return (c >= 0x2E80 && c <= 0x30FF) || (c >= 0x3400 && c <= 0x4DBF) ||
         (c >= 0x4E00 && c <= 0x9FFF) || (c >= 0xAC00 && c <= 0xD7AF) ||
         (c >= 0xF900 && c <= 0xFAFF) || (c >= 0xFF00 && c <= 0xFFEF) ||
         (c >= 0x20000 && c <= 0x3FFFF);
}

// Punctuation that must stay with the character before it.
auto is_closing(char32_t c) -> bool {
  switch (c) {
  case 0x3001: // 、
  case 0x3002: // 。
  case 0x3009: // 〉
  case 0x300B: // 》
  case 0x300D: // 」
  case 0x300F: // 』
  case 0x3011: // 】
  case 0x3015: // 〕
  case 0x30FB: // ・
  case 0x30FC: // ー
  case 0xFF01: // ！
  case 0xFF09: // ）
  case 0xFF0C: // ，
  case 0xFF0E: // ．
  case 0xFF1A: // ：
  case 0xFF1B: // ；
  case 0xFF1F: // ？
  case 0xFF3D: // ］
  case 0xFF5D: // ｝
    return true;
  default:
    return false;
  }
}

// Punctuation that must stay with the character after it.
auto is_opening(char32_t c) -> bool {
  switch (c) {
  case 0x3008: // 〈
  case 0x300A: // 《
  case 0x300C: // 「
  case 0x300E: // 『
  case 0x3010: // 【
  case 0x3014: // 〔
  case 0xFF08: // （
  case 0xFF3B: // ［
  case 0xFF5B: // ｛
    return true;
  default:
    return false;
  }
}

auto is_no_break_glue(char32_t c) -> bool {
  return c == 0x00A0 || c == 0x2007 || c == 0x202F || c == 0x2060 || c == 0xFEFF;
}

auto is_hyphen(char32_t c) -> bool { return c == U'-' || c == 0x2010 || c == 0x2013; }

auto is_word_character(char32_t c) -> bool {
  auto const category = hb_unicode_general_category(hb_unicode_funcs_get_default(), c);
  return category == HB_UNICODE_GENERAL_CATEGORY_UPPERCASE_LETTER ||
         category == HB_UNICODE_GENERAL_CATEGORY_LOWERCASE_LETTER ||
         category == HB_UNICODE_GENERAL_CATEGORY_TITLECASE_LETTER ||
         category == HB_UNICODE_GENERAL_CATEGORY_MODIFIER_LETTER ||
         category == HB_UNICODE_GENERAL_CATEGORY_OTHER_LETTER ||
         category == HB_UNICODE_GENERAL_CATEGORY_DECIMAL_NUMBER;
}

auto is_digit(char32_t c) -> bool {
  return hb_unicode_general_category(hb_unicode_funcs_get_default(), c) ==
         HB_UNICODE_GENERAL_CATEGORY_DECIMAL_NUMBER;
}

} // namespace

auto is_breaking_space(char32_t scalar) -> bool {
  return scalar == U' ' || scalar == U'\t' || scalar == 0x1680 ||
         (scalar >= 0x2000 && scalar <= 0x2006) || (scalar >= 0x2008 && scalar <= 0x200A) ||
         scalar == 0x205F || scalar == 0x3000;
}

auto break_opportunities(std::string_view utf8, std::span<std::size_t const> graphemes)
    -> std::vector<std::size_t> {
  auto breaks = std::vector<std::size_t>{};
  if (graphemes.size() < 3) {
    return breaks;
  }
  auto const lead = [&](std::size_t index) {
    return detail::decode_utf8(utf8, graphemes[index]).value;
  };
  for (auto k = std::size_t{1}; k + 1 < graphemes.size(); ++k) {
    auto const before = lead(k - 1);
    auto const after = lead(k);
    if (is_breaking_space(after) || is_no_break_glue(before) || is_no_break_glue(after) ||
        after == 0x200B) {
      continue;
    }
    auto allowed = false;
    if (is_breaking_space(before) || before == 0x200B) {
      allowed = true;
    } else if (is_hyphen(before)) {
      allowed = k >= 2 && is_word_character(lead(k - 2)) && !is_digit(after) && !is_closing(after);
    } else if (is_cjk(before) || is_cjk(after)) {
      allowed = !is_opening(before) && !is_closing(after);
    }
    if (allowed) {
      breaks.push_back(graphemes[k]);
    }
  }
  return breaks;
}

} // namespace basalt::text
