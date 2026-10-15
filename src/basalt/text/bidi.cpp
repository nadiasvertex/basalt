#include "basalt/text/bidi.h"

#include <algorithm>
#include <array>

#include "utf8.h"

namespace basalt::text {
namespace {

enum class bidi_class : std::uint8_t { l, r, al, en, es, et, an, cs, nsm, bn, b, s, ws, on };

struct class_range {
  char32_t first;
  char32_t last;
  bidi_class type;
};

using enum bidi_class;

// Sorted, non-overlapping ranges of the Unicode Bidi_Class property for the
// scripts and symbols that matter for text editing. Anything not listed is L.
constexpr auto class_table = std::to_array<class_range>({
    {0x0000, 0x0008, bn},   {0x0009, 0x0009, s},     {0x000A, 0x000A, b},    {0x000B, 0x000B, s},
    {0x000C, 0x000C, ws},   {0x000D, 0x000D, b},     {0x000E, 0x001B, bn},   {0x001C, 0x001E, b},
    {0x001F, 0x001F, s},    {0x0020, 0x0020, ws},    {0x0021, 0x0022, on},   {0x0023, 0x0025, et},
    {0x0026, 0x002A, on},   {0x002B, 0x002B, es},    {0x002C, 0x002C, cs},   {0x002D, 0x002D, es},
    {0x002E, 0x002F, cs},   {0x0030, 0x0039, en},    {0x003A, 0x003A, cs},   {0x003B, 0x0040, on},
    {0x005B, 0x0060, on},   {0x007B, 0x007E, on},    {0x007F, 0x0084, bn},   {0x0085, 0x0085, b},
    {0x0086, 0x009F, bn},   {0x00A0, 0x00A0, cs},    {0x00A1, 0x00A1, on},   {0x00A2, 0x00A5, et},
    {0x00A6, 0x00A9, on},   {0x00AB, 0x00AC, on},    {0x00AD, 0x00AD, bn},   {0x00AE, 0x00AF, on},
    {0x00B0, 0x00B1, et},   {0x00B2, 0x00B3, en},    {0x00B4, 0x00B4, on},   {0x00B6, 0x00B8, on},
    {0x00B9, 0x00B9, en},   {0x00BB, 0x00BF, on},    {0x00D7, 0x00D7, on},   {0x00F7, 0x00F7, on},
    {0x02B9, 0x02BA, on},   {0x02C2, 0x02CF, on},    {0x02D2, 0x02DF, on},   {0x02E5, 0x02ED, on},
    {0x02EF, 0x02FF, on},   {0x0300, 0x036F, nsm},   {0x0374, 0x0375, on},   {0x037E, 0x037E, on},
    {0x0384, 0x0385, on},   {0x0387, 0x0387, on},    {0x03F6, 0x03F6, on},   {0x0483, 0x0489, nsm},
    {0x058A, 0x058A, on},   {0x058D, 0x058E, on},    {0x058F, 0x058F, et},   {0x0590, 0x0590, r},
    {0x0591, 0x05BD, nsm},  {0x05BE, 0x05BE, r},     {0x05BF, 0x05BF, nsm},  {0x05C0, 0x05C0, r},
    {0x05C1, 0x05C2, nsm},  {0x05C3, 0x05C3, r},     {0x05C4, 0x05C5, nsm},  {0x05C6, 0x05C6, r},
    {0x05C7, 0x05C7, nsm},  {0x05C8, 0x05FF, r},     {0x0600, 0x0605, an},   {0x0606, 0x0607, on},
    {0x0608, 0x0608, al},   {0x0609, 0x060A, et},    {0x060B, 0x060B, al},   {0x060C, 0x060C, cs},
    {0x060D, 0x060D, al},   {0x060E, 0x060F, on},    {0x0610, 0x061A, nsm},  {0x061B, 0x064A, al},
    {0x064B, 0x065F, nsm},  {0x0660, 0x0669, an},    {0x066A, 0x066A, et},   {0x066B, 0x066C, an},
    {0x066D, 0x066F, al},   {0x0670, 0x0670, nsm},   {0x0671, 0x06D5, al},   {0x06D6, 0x06DC, nsm},
    {0x06DD, 0x06DD, an},   {0x06DE, 0x06DE, on},    {0x06DF, 0x06E4, nsm},  {0x06E5, 0x06E6, al},
    {0x06E7, 0x06E8, nsm},  {0x06E9, 0x06E9, on},    {0x06EA, 0x06ED, nsm},  {0x06EE, 0x06EF, al},
    {0x06F0, 0x06F9, en},   {0x06FA, 0x070E, al},    {0x070F, 0x070F, an},   {0x0710, 0x0710, al},
    {0x0711, 0x0711, nsm},  {0x0712, 0x072F, al},    {0x0730, 0x074A, nsm},  {0x074B, 0x07A5, al},
    {0x07A6, 0x07B0, nsm},  {0x07B1, 0x07BF, al},    {0x07C0, 0x07EA, r},    {0x07EB, 0x07F3, nsm},
    {0x07F4, 0x07F5, r},    {0x07F6, 0x07F9, on},    {0x07FA, 0x0815, r},    {0x0816, 0x082D, nsm},
    {0x082E, 0x0858, r},    {0x0859, 0x085B, nsm},   {0x085C, 0x085F, r},    {0x0860, 0x086F, al},
    {0x0870, 0x088F, al},   {0x0890, 0x0891, an},    {0x0898, 0x089F, nsm},  {0x08A0, 0x08C9, al},
    {0x08CA, 0x08E1, nsm},  {0x08E2, 0x08E2, an},    {0x08E3, 0x08FF, nsm},  {0x1AB0, 0x1AFF, nsm},
    {0x1DC0, 0x1DFF, nsm},  {0x2000, 0x200A, ws},    {0x200B, 0x200D, bn},   {0x200F, 0x200F, r},
    {0x2010, 0x2027, on},   {0x2028, 0x2028, ws},    {0x2029, 0x2029, b},    {0x202A, 0x202E, bn},
    {0x202F, 0x202F, cs},   {0x2030, 0x2034, et},    {0x2035, 0x2043, on},   {0x2044, 0x2044, cs},
    {0x2045, 0x205E, on},   {0x205F, 0x205F, ws},    {0x2060, 0x2064, bn},   {0x2066, 0x2069, on},
    {0x206A, 0x206F, bn},   {0x2070, 0x2070, en},    {0x2074, 0x2079, en},   {0x207A, 0x207B, es},
    {0x207C, 0x207E, on},   {0x2080, 0x2089, en},    {0x208A, 0x208B, es},   {0x208C, 0x208E, on},
    {0x20A0, 0x20CF, et},   {0x20D0, 0x20F0, nsm},   {0x2190, 0x2335, on},   {0x237B, 0x2394, on},
    {0x2396, 0x2426, on},   {0x2440, 0x244A, on},    {0x2460, 0x2487, on},   {0x2488, 0x249B, en},
    {0x24EA, 0x26AB, on},   {0x26AD, 0x27FF, on},    {0x2900, 0x2BFF, on},   {0x2CEF, 0x2CF1, nsm},
    {0x2CF9, 0x2CFF, on},   {0x2E00, 0x2E5D, on},    {0x2E80, 0x2FFF, on},   {0x3000, 0x3000, ws},
    {0x3001, 0x3004, on},   {0x3008, 0x3020, on},    {0x302A, 0x302D, nsm},  {0x3030, 0x3030, on},
    {0x3099, 0x309C, nsm},  {0x30A0, 0x30A0, on},    {0x30FB, 0x30FB, on},   {0xA490, 0xA4C6, on},
    {0xFB1D, 0xFB1D, r},    {0xFB1E, 0xFB1E, nsm},   {0xFB1F, 0xFB28, r},    {0xFB29, 0xFB29, es},
    {0xFB2A, 0xFB4F, r},    {0xFB50, 0xFD3D, al},    {0xFD3E, 0xFD4F, on},   {0xFD50, 0xFDCF, al},
    {0xFDD0, 0xFDEF, bn},   {0xFDF0, 0xFDFC, al},    {0xFDFD, 0xFDFD, on},   {0xFDFE, 0xFDFF, al},
    {0xFE00, 0xFE0F, nsm},  {0xFE10, 0xFE19, on},    {0xFE20, 0xFE2F, nsm},  {0xFE30, 0xFE4F, on},
    {0xFE50, 0xFE50, cs},   {0xFE51, 0xFE51, on},    {0xFE52, 0xFE52, cs},   {0xFE54, 0xFE54, on},
    {0xFE55, 0xFE55, cs},   {0xFE56, 0xFE5E, on},    {0xFE5F, 0xFE5F, et},   {0xFE60, 0xFE61, on},
    {0xFE62, 0xFE63, es},   {0xFE64, 0xFE66, on},    {0xFE68, 0xFE68, on},   {0xFE69, 0xFE6A, et},
    {0xFE6B, 0xFE6B, on},   {0xFE70, 0xFEFC, al},    {0xFEFF, 0xFEFF, bn},   {0xFF01, 0xFF02, on},
    {0xFF03, 0xFF05, et},   {0xFF06, 0xFF0A, on},    {0xFF0B, 0xFF0B, es},   {0xFF0C, 0xFF0C, cs},
    {0xFF0D, 0xFF0D, es},   {0xFF0E, 0xFF0F, cs},    {0xFF10, 0xFF19, en},   {0xFF1A, 0xFF1A, cs},
    {0xFF1B, 0xFF20, on},   {0xFF3B, 0xFF40, on},    {0xFF5B, 0xFF65, on},   {0xFFE0, 0xFFE1, et},
    {0xFFE2, 0xFFE4, on},   {0xFFE5, 0xFFE6, et},    {0xFFE8, 0xFFEE, on},   {0xFFF9, 0xFFFD, on},
    {0x10800, 0x10CFF, r},  {0x10D00, 0x10D3F, al},  {0x10D40, 0x10EBF, r},  {0x10EC0, 0x10EFF, al},
    {0x10F00, 0x10F2F, r},  {0x10F30, 0x10F6F, al},  {0x10F70, 0x10FFF, r},  {0x1E800, 0x1EDFF, r},
    {0x1EE00, 0x1EEFF, al}, {0x1EF00, 0x1EFFF, r},   {0x1F000, 0x1FAFF, on}, {0xE0001, 0xE0001, bn},
    {0xE0020, 0xE007F, bn}, {0xE0100, 0xE01EF, nsm},
});

constexpr auto table_is_sorted() -> bool {
  for (auto i = std::size_t{0}; i < class_table.size(); ++i) {
    if (class_table[i].first > class_table[i].last ||
        (i > 0 && class_table[i - 1].last >= class_table[i].first)) {
      return false;
    }
  }
  return true;
}
static_assert(table_is_sorted());

auto class_of(char32_t scalar) -> bidi_class {
  auto const it = std::ranges::upper_bound(class_table, scalar, {}, &class_range::first);
  if (it != class_table.begin() && scalar <= std::prev(it)->last) {
    return std::prev(it)->type;
  }
  return l;
}

auto is_strong(bidi_class type) -> bool { return type == l || type == r || type == al; }
auto is_neutral(bidi_class type) -> bool {
  return type == b || type == s || type == ws || type == on;
}

struct unit {
  std::size_t begin;
  std::size_t end;
  bidi_class original;
  bidi_class type;
  std::uint8_t level = 0;
};

auto first_strong_level(std::vector<unit> const &units) -> std::uint8_t {
  for (auto const &item : units) {
    if (item.original == l) {
      return 0;
    }
    if (item.original == r || item.original == al) {
      return 1;
    }
  }
  return 0;
}

// W1-W7 over the units that take part in the algorithm. `sos` is the direction
// of the paragraph.
auto resolve_weak(std::vector<unit *> &units, bidi_class sos) -> void {
  auto previous = sos;
  for (auto *item : units) {
    if (item->type == nsm) {
      item->type = previous;
    }
    previous = item->type;
  }

  auto last_strong = sos;
  for (auto *item : units) {
    if (is_strong(item->type)) {
      last_strong = item->type;
    } else if (item->type == en && last_strong == al) {
      item->type = an;
    }
  }
  for (auto *item : units) {
    if (item->type == al) {
      item->type = r;
    }
  }

  for (auto i = std::size_t{1}; i + 1 < units.size(); ++i) {
    auto &type = units[i]->type;
    auto const before = units[i - 1]->type;
    auto const after = units[i + 1]->type;
    if (type == es && before == en && after == en) {
      type = en;
    } else if (type == cs && before == after && (before == en || before == an)) {
      type = before;
    }
  }

  for (auto i = std::size_t{0}; i < units.size();) {
    if (units[i]->type != et) {
      ++i;
      continue;
    }
    auto end = i;
    while (end < units.size() && units[end]->type == et) {
      ++end;
    }
    auto const adjacent_number =
        (i > 0 && units[i - 1]->type == en) || (end < units.size() && units[end]->type == en);
    for (auto k = i; k < end; ++k) {
      units[k]->type = adjacent_number ? en : on;
    }
    i = end;
  }
  for (auto *item : units) {
    if (item->type == es || item->type == cs) {
      item->type = on;
    }
  }

  last_strong = sos;
  for (auto *item : units) {
    if (item->type == l || item->type == r) {
      last_strong = item->type;
    } else if (item->type == en && last_strong == l) {
      item->type = l;
    }
  }
}

auto direction_of(bidi_class type) -> bidi_class { return (type == en || type == an) ? r : type; }

// N1-N2: neutrals take the direction of the strong text on both sides, and
// the embedding direction otherwise.
auto resolve_neutral(std::vector<unit *> &units, bidi_class sos, bidi_class embedding) -> void {
  for (auto i = std::size_t{0}; i < units.size();) {
    if (!is_neutral(units[i]->type)) {
      ++i;
      continue;
    }
    auto end = i;
    while (end < units.size() && is_neutral(units[end]->type)) {
      ++end;
    }
    auto const before = i > 0 ? direction_of(units[i - 1]->type) : sos;
    auto const after = end < units.size() ? direction_of(units[end]->type) : sos;
    auto const resolved = before == after ? before : embedding;
    for (auto k = i; k < end; ++k) {
      units[k]->type = resolved;
    }
    i = end;
  }
}

auto implicit_level(std::uint8_t base, bidi_class type) -> std::uint8_t {
  if ((base & 1U) == 0) {
    if (type == r) {
      return base + 1;
    }
    if (type == an || type == en) {
      return base + 2;
    }
    return base;
  }
  return (type == l || type == en || type == an) ? base + 1 : base;
}

} // namespace

auto resolve_bidi(std::string_view utf8, bidi_base base) -> bidi_paragraph {
  // ASCII has no right-to-left characters, so a left-to-right paragraph of it is one level-zero
  // run. The weak and neutral rules would give the same answer.
  if (base != bidi_base::right_to_left && !utf8.empty() &&
      std::ranges::all_of(utf8, [](char c) { return static_cast<unsigned char>(c) < 0x80; })) {
    return {0, {{0, utf8.size(), 0}}};
  }
  auto units = std::vector<unit>{};
  for (auto pos = std::size_t{0}; pos < utf8.size();) {
    auto const decoded = detail::decode_utf8(utf8, pos);
    auto const type = class_of(decoded.value);
    units.push_back({pos, pos + decoded.length, type, type});
    pos += decoded.length;
  }

  auto result = bidi_paragraph{};
  switch (base) {
  case bidi_base::left_to_right:
    result.base_level = 0;
    break;
  case bidi_base::right_to_left:
    result.base_level = 1;
    break;
  case bidi_base::automatic:
    result.base_level = first_strong_level(units);
    break;
  }
  auto const embedding = (result.base_level & 1U) != 0 ? r : l;

  // Boundary-neutral characters do not take part; they inherit a level below.
  auto active = std::vector<unit *>{};
  for (auto &item : units) {
    if (item.type != bn) {
      active.push_back(&item);
    }
  }
  resolve_weak(active, embedding);
  resolve_neutral(active, embedding, embedding);
  for (auto *item : active) {
    item->level = implicit_level(result.base_level, item->type);
  }

  // L1: separators and the whitespace before them or the end of the line.
  auto trailing = true;
  for (auto it = active.rbegin(); it != active.rend(); ++it) {
    auto &item = **it;
    if (item.original == b || item.original == s) {
      item.level = result.base_level;
      trailing = true;
    } else if (item.original == ws && trailing) {
      item.level = result.base_level;
    } else {
      trailing = false;
    }
  }

  auto previous_level = result.base_level;
  for (auto &item : units) {
    if (item.original == bn) {
      item.level = previous_level;
    }
    previous_level = item.level;
  }

  for (auto const &item : units) {
    if (!result.runs.empty() && result.runs.back().level == item.level) {
      result.runs.back().end = item.end;
    } else {
      result.runs.push_back({item.begin, item.end, item.level});
    }
  }
  return result;
}

auto visual_order(std::vector<bidi_run> const &runs) -> std::vector<std::size_t> {
  auto order = std::vector<std::size_t>(runs.size());
  for (auto i = std::size_t{0}; i < order.size(); ++i) {
    order[i] = i;
  }
  auto highest = std::uint8_t{0};
  auto lowest_odd = std::uint8_t{255};
  for (auto const &run : runs) {
    highest = std::max(highest, run.level);
    if ((run.level & 1U) != 0) {
      lowest_odd = std::min(lowest_odd, run.level);
    }
  }
  for (auto level = highest; level >= lowest_odd && level > 0; --level) {
    for (auto i = std::size_t{0}; i < order.size();) {
      if (runs[order[i]].level < level) {
        ++i;
        continue;
      }
      auto end = i;
      while (end < order.size() && runs[order[end]].level >= level) {
        ++end;
      }
      std::reverse(order.begin() + static_cast<std::ptrdiff_t>(i),
                   order.begin() + static_cast<std::ptrdiff_t>(end));
      i = end;
    }
  }
  return order;
}

} // namespace basalt::text
