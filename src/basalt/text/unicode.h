#pragma once

#include <hb.h>

namespace basalt::text::detail {

[[nodiscard]] inline auto is_mark(hb_unicode_general_category_t category) -> bool {
  return category == HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK ||
         category == HB_UNICODE_GENERAL_CATEGORY_ENCLOSING_MARK ||
         category == HB_UNICODE_GENERAL_CATEGORY_SPACING_MARK;
}

[[nodiscard]] inline auto is_mark(char32_t c) -> bool {
  return is_mark(hb_unicode_general_category(hb_unicode_funcs_get_default(), c));
}

} // namespace basalt::text::detail
