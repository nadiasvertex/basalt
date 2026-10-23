#include "basalt/text/paragraph.h"

#include "basalt/text/line_break.h"

#include <hb.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <span>

#include "glyph_clusters.h"
#include "unicode.h"
#include "utf8.h"

namespace basalt::text {
namespace {

// Slack for float rounding when comparing a measured width to the limit.
constexpr auto width_epsilon = 0.01F;

auto is_extender(char32_t c) -> bool {
  if (c == 0x200C || c == 0x200D || (c >= 0x1F3FB && c <= 0x1F3FF) ||
      (c >= 0xE0020 && c <= 0xE007F)) {
    return true;
  }
  return detail::is_mark(c);
}

auto is_regional_indicator(char32_t c) -> bool { return c >= 0x1F1E6 && c <= 0x1F1FF; }

auto is_pictographic(char32_t c) -> bool {
  return (c >= 0x1F000 && c <= 0x1FAFF) || (c >= 0x2600 && c <= 0x27BF) ||
         (c >= 0x2190 && c <= 0x21FF) || (c >= 0x2B00 && c <= 0x2BFF);
}

auto gap_at(std::span<inline_gap const> gaps, std::size_t offset) -> float {
  auto const found = std::ranges::lower_bound(gaps, offset, {}, &inline_gap::offset);
  return found != gaps.end() && found->offset == offset ? found->width : 0.0F;
}

// The width of the gaps in [begin, end), and the one at `end` when `with_end` is set.
auto gap_total(std::span<inline_gap const> gaps, std::size_t begin, std::size_t end, bool with_end)
    -> float {
  auto total = 0.0F;
  auto it = std::ranges::lower_bound(gaps, begin, {}, &inline_gap::offset);
  for (; it != gaps.end() && it->offset < end; ++it) {
    total += it->width;
  }
  if (with_end && it != gaps.end() && it->offset == end) {
    total += it->width;
  }
  return total;
}

struct hard_line {
  std::size_t begin = 0;
  std::size_t end = 0;  // Before the break.
  std::size_t next = 0; // After the break.
};

auto split_hard_lines(std::string_view text) -> std::vector<hard_line> {
  auto result = std::vector<hard_line>{};
  auto begin = std::size_t{0};
  for (auto i = std::size_t{0}; i < text.size(); ++i) {
    if (text[i] != '\n' && text[i] != '\r') {
      continue;
    }
    auto next = i + 1;
    if (text[i] == '\r' && next < text.size() && text[next] == '\n') {
      ++next;
    }
    result.push_back({begin, i, next});
    begin = next;
    i = next - 1;
  }
  result.push_back({begin, text.size(), text.size()});
  return result;
}

// The largest index in [0, count) for which `fits` holds, assuming it holds
// for a prefix of the indices, or -1. Probes at growing distances first so a
// short answer on a very long line costs few evaluations.
template <class F> auto largest_fitting(std::ptrdiff_t count, F const &fits) -> std::ptrdiff_t {
  auto lo = std::ptrdiff_t{-1};
  auto hi = count;
  auto probe = std::ptrdiff_t{0};
  auto step = std::ptrdiff_t{1};
  while (probe < hi) {
    if (!fits(probe)) {
      hi = probe;
      break;
    }
    lo = probe;
    probe += step;
    step *= 2;
  }
  while (hi - lo > 1) {
    auto const mid = lo + (hi - lo) / 2;
    if (fits(mid)) {
      lo = mid;
    } else {
      hi = mid;
    }
  }
  return lo;
}

} // namespace

auto default_grapheme_boundaries(std::string_view utf8) -> std::vector<std::size_t> {
  auto boundaries = std::vector<std::size_t>{};
  auto previous = char32_t{0};
  auto regional_indicators = 0;
  for (auto pos = std::size_t{0}; pos < utf8.size();) {
    auto const decoded = detail::decode_utf8(utf8, pos);
    auto const c = decoded.value;
    auto joins = false;
    if (pos > 0) {
      joins = is_extender(c) || (previous == U'\r' && c == U'\n') ||
              (previous == 0x200D && is_pictographic(c)) ||
              (is_regional_indicator(c) && is_regional_indicator(previous) &&
               regional_indicators % 2 == 1);
    }
    if (!joins) {
      boundaries.push_back(pos);
    }
    regional_indicators = is_regional_indicator(c) ? regional_indicators + 1 : 0;
    previous = c;
    pos += decoded.length;
  }
  boundaries.push_back(utf8.size());
  return boundaries;
}

paragraph::paragraph(text_shaper const &shaper, std::string_view utf8,
                     paragraph_options const &options)
    : text_size_(utf8.size()),
      break_marker_width_(options.shape.style.font_size * detail::break_marker_em),
      line_padding_(std::max(0.0F, options.line_padding)), gaps_(options.gaps) {
  std::ranges::sort(gaps_, {}, &inline_gap::offset);
  if (options.graphemes) {
    graphemes_ = options.graphemes(utf8);
  }
  if (graphemes_.empty() || graphemes_.front() != 0 || graphemes_.back() != utf8.size() ||
      !std::ranges::is_sorted(graphemes_)) {
    graphemes_ = default_grapheme_boundaries(utf8);
  }
  break_into_lines(shaper, utf8, options);
  for (auto const &line : lines_) {
    extent_.width = std::max(extent_.width, line.width);
  }
  extent_.width += std::max(0.0F, options.indent);
  extent_.height = lines_.empty() ? 0 : lines_.back().y + lines_.back().height;
}

namespace {

// The shape options for the part [begin, end) of the paragraph, with the style spans cut to it
// and counted from its start.
auto rebased(shape_options const &options, std::size_t begin, std::size_t end) -> shape_options {
  if (options.spans.empty()) {
    return options;
  }
  auto result = options;
  result.spans.clear();
  for (auto const &span : options.spans) {
    auto const from = std::max(span.begin, begin);
    auto const to = std::min(span.end, end);
    if (from < to) {
      auto rebased_span = span;
      rebased_span.begin = from - begin;
      rebased_span.end = to - begin;
      result.spans.push_back(rebased_span);
    }
  }
  return result;
}

// Where the line that starts at `start` ends: the last break opportunity that fits, else the
// last grapheme that fits, else the first grapheme.
template <class Fits>
auto choose_line_end(std::span<std::size_t const> breaks, std::span<std::size_t const> graphemes,
                     hard_line const &hard, std::size_t start, Fits const &fits) -> std::size_t {
  auto const first = std::ranges::upper_bound(breaks, start);
  auto const last = std::ranges::lower_bound(breaks, hard.end);
  // Candidates are the break opportunities in the line, then the hard end. The search probes at
  // growing distances, so the rest of a very long hard line is not shaped for every line that is
  // cut from it.
  auto const count = last - first;
  auto const candidate = [&](std::ptrdiff_t i) { return i < count ? first[i] : hard.end; };
  auto const index =
      largest_fitting(count + 1, [&](std::ptrdiff_t i) { return fits(start, candidate(i)); });
  if (index >= 0) {
    return candidate(index);
  }
  // One word is wider than the line: break between graphemes.
  auto const limit = first != last ? *first : hard.end;
  auto const from = std::ranges::upper_bound(graphemes, start);
  auto const to = std::ranges::lower_bound(graphemes, limit);
  auto const inside =
      largest_fitting(to - from, [&](std::ptrdiff_t i) { return fits(start, from[i]); });
  return inside >= 0 ? from[inside] : *from;
}

// Fills in the vertical metrics of `line` from the shaped text.
auto apply_line_metrics(paragraph_line &line, shaped_text const &shaped, float line_height,
                        float line_padding) -> void {
  line.ascent = shaped.ascent;
  line.descent = shaped.descent;
  auto const natural = shaped.ascent + shaped.descent + shaped.line_gap;
  auto const text_height = line_height > 0 ? line_height : natural;
  line.height = text_height + line_padding;
  line.baseline = (text_height - (shaped.ascent + shaped.descent)) / 2 + shaped.ascent;
}

// The x offset of a line of `content` width in the `available` width, and `hang` of trailing
// spaces that sit on the side the paragraph ends on.
auto alignment_offset(text_align align, float available, float content, float hang,
                      bool right_to_left) -> float {
  if (!std::isfinite(available)) {
    return 0.0F;
  }
  auto const towards_start = align == text_align::start;
  auto const towards_end = align == text_align::end;
  auto const left_aligned = (towards_start && !right_to_left) || (towards_end && right_to_left);
  auto const right_aligned = (towards_start && right_to_left) || (towards_end && !right_to_left);
  auto left = (available - content) / 2;
  if (left_aligned) {
    left = 0;
  } else if (right_aligned) {
    left = available - content;
  }
  return left - (right_to_left ? hang : 0);
}

// Gaps push everything after them aside, so each glyph moves by the gaps before it in display
// order. `shift` is the total width of the gaps in the runs so far.
auto apply_gaps(glyph_run &run, std::span<inline_gap const> gaps, float &shift) -> void {
  auto const run_shift = shift;
  auto const rtl = run.right_to_left();
  for (auto cluster : detail::clusters(run)) {
    auto const width = gap_at(gaps, cluster.front().cluster);
    for (auto &g : cluster) {
      g.position.x += (shift - run_shift) + (rtl ? 0.0F : width);
    }
    // The gap is on the side the cluster starts on: left in a left-to-right run.
    (rtl ? cluster.back() : cluster.front()).advance += width;
    shift += width;
  }
  run.advance += shift - run_shift;
}

} // namespace

auto paragraph::break_into_lines(text_shaper const &shaper, std::string_view utf8,
                                 paragraph_options const &options) -> void {
  auto const limited = options.wrap && std::isfinite(options.max_width);
  auto const indent = std::max(0.0F, options.indent);
  auto const available = std::max(0.0F, options.max_width - indent);
  auto const breaks = limited ? break_opportunities(utf8, graphemes_) : std::vector<std::size_t>{};
  auto const trim_spaces = [&](std::size_t begin, std::size_t end) {
    while (end > begin) {
      auto const previous = *std::prev(std::ranges::lower_bound(graphemes_, end));
      if (previous < begin || !is_breaking_space(detail::decode_utf8(utf8, previous).value)) {
        break;
      }
      end = previous;
    }
    return end;
  };

  auto y = 0.0F;
  for (auto const &hard : split_hard_lines(utf8)) {
    auto shape = options.shape;
    auto const level =
        resolve_bidi(utf8.substr(hard.begin, hard.end - hard.begin), options.shape.direction)
            .base_level;
    shape.direction = (level & 1U) != 0 ? bidi_base::right_to_left : bidi_base::left_to_right;
    auto const measure = [&](std::size_t begin, std::size_t end) {
      auto const trimmed = trim_spaces(begin, end);
      return shaper.shape(utf8.substr(begin, trimmed - begin), rebased(shape, begin, trimmed))
                 .width +
             gap_total(gaps_, begin, trimmed, trimmed == hard.end);
    };
    auto const fits = [&](std::size_t begin, std::size_t end) {
      return measure(begin, end) <= available + width_epsilon;
    };

    auto start = hard.begin;
    do {
      auto const end = limited ? choose_line_end(breaks, graphemes_, hard, start, fits) : hard.end;

      auto line = paragraph_line{};
      line.begin = start;
      line.end = end;
      line.wrapped = end < hard.end;
      line.base_level = level;
      line.y = y;

      auto shaped = shaper.shape(utf8.substr(start, end - start), rebased(shape, start, end));
      line.width = shaped.width + gap_total(gaps_, start, end, end == hard.end);
      apply_line_metrics(line, shaped, shape.style.line_height, line_padding_);

      // Spaces that hang past a soft wrap do not count toward alignment. They
      // sit on the side the paragraph ends on.
      auto hang = 0.0F;
      if (line.wrapped) {
        auto const trimmed = trim_spaces(start, end);
        for (auto const &placed : shaped.runs) {
          for (auto const &g : placed.run->glyphs) {
            if (g.cluster + start >= trimmed) {
              hang += g.advance;
            }
          }
        }
      }
      auto const content = line.width - hang;
      auto const right_to_left = (level & 1U) != 0;
      line.x = alignment_offset(options.align, available, content, hang, right_to_left) +
               (right_to_left ? 0.0F : indent);

      line.runs.reserve(shaped.runs.size());
      auto shift = 0.0F;
      for (auto const &placed : shaped.runs) {
        auto run = std::make_shared<glyph_run>(*placed.run);
        detail::offset_clusters(*run, start);
        auto const run_shift = shift;
        if (!gaps_.empty()) {
          apply_gaps(*run, gaps_, shift);
        }
        line.runs.push_back({std::move(run), placed.x + run_shift + line.x});
      }

      y += line.height;
      lines_.push_back(std::move(line));
      start = end;
    } while (start < hard.end);
  }
}

auto merge_touching_rects(std::vector<rect> rects) -> std::vector<rect> {
  std::ranges::sort(rects, [](rect const &a, rect const &b) {
    return a.top() != b.top() ? a.top() < b.top() : a.left() < b.left();
  });
  auto merged = std::vector<rect>{};
  for (auto const &r : rects) {
    if (!merged.empty() && merged.back().top() == r.top() &&
        r.left() <= merged.back().right() + width_epsilon) {
      merged.back() = rect::from_edges(merged.back().left(), r.top(),
                                       std::max(merged.back().right(), r.right()), r.bottom());
    } else {
      merged.push_back(r);
    }
  }
  return merged;
}

auto paragraph::append_run_stops(positioned_run const &placed, std::vector<stop> &out) const
    -> void {
  auto const &run = *placed.run;
  auto const rtl = run.right_to_left();
  auto pen = 0.0F;
  auto previous = run.text_end;
  auto chunks = detail::clusters(run);
  for (auto it = chunks.begin(); it != chunks.end(); ++it) {
    auto const cluster = *it;
    auto width = 0.0F;
    for (auto const &g : cluster) {
      width += g.advance;
    }
    auto const first = static_cast<std::size_t>(cluster.front().cluster);
    // The gap before the cluster is part of its advance, and the caret stops follow it.
    auto const lead = gap_at(gaps_, first);
    auto const next = std::next(it);
    auto const following =
        next != chunks.end() ? static_cast<std::size_t>((*next).front().cluster) : run.text_end;
    auto const last = rtl ? previous : following;
    auto const from = std::ranges::lower_bound(graphemes_, first);
    auto const to = std::ranges::lower_bound(graphemes_, last);
    auto const count = to - from;
    for (auto k = std::ptrdiff_t{0}; k < count; ++k) {
      auto const share = (width - lead) * static_cast<float>(k) / static_cast<float>(count);
      auto const x = rtl ? pen + width - lead - share : pen + lead + share;
      out.push_back({from[k], placed.x + x, true, from[k] > run.text_begin});
    }
    pen += width;
    previous = first;
  }
  out.push_back({run.text_end, placed.x + (rtl ? 0.0F : run.advance), false, true});
}

auto paragraph::line_stops(paragraph_line const &line) const -> std::vector<stop> {
  auto stops = std::vector<stop>{};
  if (line.runs.empty()) {
    auto const lead = line.begin == line.end ? gap_at(gaps_, line.begin) : 0.0F;
    stops.push_back({line.begin, line.x + lead, true, line.begin == line.end});
    if (line.end != line.begin) {
      stops.push_back({line.end, line.x + line.width, false, true});
    }
    return stops;
  }
  for (auto const &placed : line.runs) {
    append_run_stops(placed, stops);
  }
  return stops;
}

auto paragraph::line_of(std::size_t offset, caret_affinity affinity) const -> std::size_t {
  offset = std::min(offset, text_size_);
  auto const after = std::ranges::upper_bound(lines_, offset, {}, &paragraph_line::begin);
  auto index = static_cast<std::size_t>(after - lines_.begin());
  index = index == 0 ? 0 : index - 1;
  if (affinity == caret_affinity::upstream && index > 0 && lines_[index - 1].wrapped &&
      lines_[index].begin == offset) {
    --index;
  }
  return index;
}

auto paragraph::line_at_y(float y) const -> std::size_t {
  auto const after = std::ranges::upper_bound(lines_, y, {}, &paragraph_line::y);
  auto const index = static_cast<std::size_t>(after - lines_.begin());
  return index == 0 ? 0 : index - 1;
}

auto paragraph::hit_test_line(std::size_t line_index, float x) const -> text_hit {
  line_index = std::min(line_index, lines_.size() - 1);
  auto const &line = lines_[line_index];
  auto const stops = line_stops(line);
  auto best = std::ranges::min_element(stops, {}, [&](stop const &s) { return std::abs(s.x - x); });
  return {best->offset, best->leading ? caret_affinity::downstream : caret_affinity::upstream,
          line_index, x >= line.x && x <= line.x + line.width};
}

auto paragraph::hit_test(point position) const -> text_hit {
  auto const index = line_at_y(position.y);
  auto result = hit_test_line(index, position.x);
  result.inside = result.inside && position.y >= lines_[index].y &&
                  position.y < lines_[index].y + lines_[index].height;
  return result;
}

auto paragraph::caret_rect(std::size_t offset, caret_affinity affinity) const -> rect {
  offset = std::min(offset, text_size_);
  auto const boundary = std::ranges::upper_bound(graphemes_, offset);
  offset = *std::prev(boundary);
  auto const &line = lines_[line_of(offset, affinity)];
  auto const stops = line_stops(line);
  auto const wanted = [&](stop const &s) {
    return s.offset == offset && (affinity == caret_affinity::downstream ? s.leading : s.trailing);
  };
  auto found = std::ranges::find_if(stops, wanted);
  if (found == stops.end()) {
    found = std::ranges::find(stops, offset, &stop::offset);
  }
  auto const x = found == stops.end() ? line.x : found->x;
  return {{x, line.y}, {0, line.height}};
}

auto paragraph::gap_rect(std::size_t offset) const -> std::optional<rect> {
  auto const width = gap_at(gaps_, offset);
  if (width <= 0 || offset > text_size_) {
    return std::nullopt;
  }
  auto const &line = lines_[line_of(offset)];
  auto const height = line.height - line_padding_;
  auto const stops = line_stops(line);
  // A gap in front of a character ends at the caret stop before that character. A gap at the end of
  // the text starts at the last stop.
  auto const leading =
      std::ranges::find_if(stops, [&](stop const &s) { return s.offset == offset && s.leading; });
  if (leading != stops.end()) {
    auto const rtl = !line.runs.empty() && line.runs.front().run->right_to_left();
    return rect{{rtl ? leading->x : leading->x - width, line.y}, {width, height}};
  }
  auto const trailing =
      std::ranges::find_if(stops, [&](stop const &s) { return s.offset == offset && s.trailing; });
  if (trailing == stops.end()) {
    return std::nullopt;
  }
  return rect{{trailing->x, line.y}, {width, height}};
}

auto paragraph::selection_rects(std::size_t begin, std::size_t end) const -> std::vector<rect> {
  end = std::min(end, text_size_);
  auto rects = std::vector<rect>{};
  if (begin >= end) {
    return rects;
  }
  for (auto index = line_of(begin); index < lines_.size() && lines_[index].begin <= end; ++index) {
    auto const &line = lines_[index];
    auto const from = std::max(begin, line.begin);
    auto const to = std::min(end, line.end);
    for (auto const &placed : line.runs) {
      auto const &run = *placed.run;
      auto const a = std::max(from, run.text_begin);
      auto const b = std::min(to, run.text_end);
      if (a >= b) {
        continue;
      }
      auto stops = std::vector<stop>{};
      append_run_stops(placed, stops);
      auto const x_at = [&](std::size_t offset, bool leading) {
        auto exact = std::ranges::find_if(stops, [&](stop const &s) {
          return s.offset == offset && (leading ? s.leading : s.trailing);
        });
        if (exact != stops.end()) {
          return exact->x;
        }
        // Not a grapheme boundary of this run: use the closest one before it.
        auto best = stops.end();
        for (auto it = stops.begin(); it != stops.end(); ++it) {
          if (it->offset <= offset && (best == stops.end() || it->offset > best->offset)) {
            best = it;
          }
        }
        return best == stops.end() ? placed.x : best->x;
      };
      auto const xa = x_at(a, true);
      auto const xb = x_at(b, false);
      rects.push_back({{std::min(xa, xb), line.y}, {std::abs(xb - xa), line.height}});
    }
    auto const hard_break = !line.wrapped && index + 1 < lines_.size();
    if (hard_break && begin <= line.end && end > line.end) {
      auto const stops = line_stops(line);
      auto const found = std::ranges::find_if(
          stops, [&](stop const &s) { return s.offset == line.end && s.trailing; });
      auto const x = found == stops.end() ? line.x : found->x;
      auto const left = line.right_to_left() ? x - break_marker_width_ : x;
      rects.push_back({{left, line.y}, {break_marker_width_, line.height}});
    }
  }

  return merge_touching_rects(std::move(rects));
}

} // namespace basalt::text
