#include "basalt/text/text_shaper.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <span>

#include "font_face_impl.h"
#include "utf8.h"

namespace basalt::text {
namespace {

// HarfBuzz positions are 26.6 fixed point at this scale.
constexpr auto shaping_scale = 64;

struct scalar_info {
  std::size_t begin = 0;
  std::size_t end = 0;
  char32_t value = 0;
  hb_script_t script = HB_SCRIPT_COMMON;
  bool attached = false; // Marks and joiners belong to the previous character's font.
  bool common = false;   // Script-neutral text stays in the surrounding font.
};

struct segment {
  std::size_t begin = 0;
  std::size_t end = 0;
  hb_script_t script = HB_SCRIPT_COMMON;
  font_face_ptr face;
};

auto is_neutral_script(hb_script_t script) -> bool {
  return script == HB_SCRIPT_COMMON || script == HB_SCRIPT_INHERITED || script == HB_SCRIPT_UNKNOWN;
}

auto append_utf8(std::string &out, char32_t c) -> void {
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

// The text with each character replaced by its capital. A character whose capital has another
// length keeps its own bytes, so offsets in the result are offsets in the text.
auto capitalized(std::string_view text, char32_t (*capital)(char32_t)) -> std::string {
  std::string out;
  for (auto pos = std::size_t{0}; pos < text.size();) {
    auto const decoded = detail::decode_utf8(text, pos);
    auto mapped = std::string{};
    if (decoded.value != detail::replacement_character) {
      append_utf8(mapped, capital(decoded.value));
    }
    if (mapped.empty() || mapped.size() != decoded.length) {
      out.append(text.substr(pos, decoded.length));
    } else {
      out += mapped;
    }
    pos += decoded.length;
  }
  return out;
}

// Scalars of [begin, end) with scripts resolved: neutral characters take the
// script of the text before them, or of the first script after them.
auto scan_scalars(std::string_view text, std::size_t begin, std::size_t end)
    -> std::vector<scalar_info> {
  auto *const unicode = hb_unicode_funcs_get_default();
  auto scalars = std::vector<scalar_info>{};
  for (auto pos = begin; pos < end;) {
    auto const decoded = detail::decode_utf8(text, pos);
    auto item = scalar_info{};
    item.begin = pos;
    item.end = pos + decoded.length;
    item.value = decoded.value;
    item.script = hb_unicode_script(unicode, decoded.value);
    item.common = is_neutral_script(item.script);
    auto const category = hb_unicode_general_category(unicode, decoded.value);
    item.attached = category == HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK ||
                    category == HB_UNICODE_GENERAL_CATEGORY_ENCLOSING_MARK ||
                    category == HB_UNICODE_GENERAL_CATEGORY_SPACING_MARK ||
                    category == HB_UNICODE_GENERAL_CATEGORY_FORMAT;
    scalars.push_back(item);
    pos = item.end;
  }

  auto current = HB_SCRIPT_COMMON;
  for (auto &item : scalars) {
    if (item.common) {
      item.script = current;
    } else {
      current = item.script;
    }
  }
  auto const first_real =
      std::ranges::find_if(scalars, [](scalar_info const &item) { return !item.common; });
  if (first_real != scalars.end()) {
    for (auto it = scalars.begin(); it != first_real; ++it) {
      it->script = first_real->script;
    }
  }
  return scalars;
}

class segmenter {
public:
  segmenter(font_manager &fonts, font_request const &request, font_face_ptr primary)
      : fonts_(&fonts), request_(&request), primary_(std::move(primary)) {}

  [[nodiscard]] auto split(std::vector<scalar_info> const &scalars) const -> std::vector<segment> {
    auto segments = std::vector<segment>{};
    auto previous = font_face_ptr{};
    for (auto first = std::size_t{0}; first < scalars.size();) {
      // A base character and the marks and joiners after it must share a font,
      // or the marks would not attach to the base.
      auto last = first + 1;
      while (last < scalars.size() && scalars[last].attached) {
        ++last;
      }
      auto const cluster = std::span{scalars}.subspan(first, last - first);
      auto face = choose_face(cluster, previous);
      auto const &lead = cluster.front();
      auto const end = cluster.back().end;
      if (segments.empty() || segments.back().face != face ||
          segments.back().script != lead.script) {
        segments.push_back({lead.begin, end, lead.script, face});
      } else {
        segments.back().end = end;
      }
      previous = std::move(face);
      first = last;
    }
    return segments;
  }

private:
  static auto covers_all(font_face_ptr const &face, std::span<scalar_info const> cluster) -> bool {
    return face != nullptr && std::ranges::all_of(cluster, [&](scalar_info const &item) {
             return face->covers(item.value);
           });
  }

  [[nodiscard]] auto choose_face(std::span<scalar_info const> cluster,
                                 font_face_ptr const &previous) const -> font_face_ptr {
    auto const &base = cluster.front();
    // Script-neutral text such as spaces and punctuation stays in the
    // surrounding font rather than splitting the run.
    auto const preferred = std::array{base.common ? previous : nullptr, primary_};
    for (auto const &candidate : preferred) {
      if (covers_all(candidate, cluster)) {
        return candidate;
      }
    }
    auto text = std::u32string{};
    for (auto const &item : cluster) {
      text.push_back(item.value);
    }
    if (auto fallback = fonts_->fallback_for(text, *request_)) {
      return fallback;
    }
    // No font has the whole cluster, so the base character decides.
    for (auto const &candidate : preferred) {
      if (candidate != nullptr && candidate->covers(base.value)) {
        return candidate;
      }
    }
    if (auto fallback = fonts_->fallback_for(base.value, *request_)) {
      return fallback;
    }
    return primary_;
  }

  font_manager *fonts_;
  font_request const *request_;
  font_face_ptr primary_;
};

struct buffer_deleter {
  auto operator()(hb_buffer_t *buffer) const -> void { hb_buffer_destroy(buffer); }
};

auto shape_segment(hb_buffer_t &buffer, std::string_view text, segment const &part,
                   bidi_run const &bidi, shape_options const &options) -> glyph_run {
  auto &state = native_impl(*part.face);
  auto const size = options.style.font_size;
  auto const scaled = static_cast<int>(std::lround(size * shaping_scale));
  hb_font_set_scale(state.shape_font.get(), scaled, scaled);

  hb_buffer_clear_contents(&buffer);
  hb_buffer_add_utf8(&buffer, text.data(), static_cast<int>(text.size()),
                     static_cast<unsigned>(part.begin), static_cast<int>(part.end - part.begin));
  hb_buffer_set_direction(&buffer, bidi.right_to_left() ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
  if (!is_neutral_script(part.script)) {
    hb_buffer_set_script(&buffer, part.script);
  }
  hb_buffer_set_language(&buffer,
                         options.language.empty()
                             ? hb_language_get_default()
                             : hb_language_from_string(options.language.data(),
                                                       static_cast<int>(options.language.size())));

  // Ligatures would hide the spacing between the letters they join.
  auto const spaced = options.style.letter_spacing != 0;
  auto features = std::array<hb_feature_t, 2>{};
  if (spaced) {
    hb_feature_from_string("-liga", -1, &features[0]);
    hb_feature_from_string("-clig", -1, &features[1]);
  }
  hb_shape(state.shape_font.get(), &buffer, features.data(), spaced ? 2U : 0U);

  auto count = 0U;
  auto const *infos = hb_buffer_get_glyph_infos(&buffer, &count);
  auto const *positions = hb_buffer_get_glyph_positions(&buffer, &count);
  // Joined scripts connect letters, so spacing between them would break words.
  auto const letter_spacing = part.script == HB_SCRIPT_ARABIC || part.script == HB_SCRIPT_SYRIAC ||
                                      part.script == HB_SCRIPT_MONGOLIAN ||
                                      part.script == HB_SCRIPT_NKO
                                  ? 0.0F
                                  : options.style.letter_spacing;

  auto run = glyph_run{};
  run.face = part.face;
  run.size = size;
  run.level = bidi.level;
  run.text_begin = part.begin;
  run.text_end = part.end;
  run.glyphs.reserve(count);
  auto pen = 0.0F;
  auto const unit = 1.0F / static_cast<float>(shaping_scale);
  for (auto i = 0U; i < count; ++i) {
    auto const advance = static_cast<float>(positions[i].x_advance) * unit + letter_spacing;
    run.glyphs.push_back({infos[i].codepoint,
                          {pen + static_cast<float>(positions[i].x_offset) * unit,
                           -static_cast<float>(positions[i].y_offset) * unit},
                          advance,
                          infos[i].cluster});
    pen += advance;
  }
  run.advance = pen;
  return run;
}

} // namespace

auto text_shaper::shape(std::string_view utf8, shape_options const &options) const -> shaped_text {
  if (!options.spans.empty()) {
    return shape_spans(utf8, options);
  }
  auto result = shaped_text{};
  auto const request = font_request{std::string{options.style.family},
                                    static_cast<int>(options.style.weight), options.italic};
  auto const primary = fonts_->primary_face(request);
  if (primary == nullptr) {
    return result;
  }

  auto const paragraph = resolve_bidi(utf8, options.direction);
  result.base_level = paragraph.base_level;
  auto const split = segmenter{*fonts_, request, primary};
  auto const buffer = std::unique_ptr<hb_buffer_t, buffer_deleter>{hb_buffer_create()};

  auto const extents = primary->metrics(options.style.font_size);
  result.ascent = extents.ascent;
  result.descent = extents.descent;
  result.line_gap = extents.line_gap;

  auto x = 0.0F;
  for (auto const index : visual_order(paragraph.runs)) {
    auto const &bidi = paragraph.runs[index];
    auto segments = split.split(scan_scalars(utf8, bidi.begin, bidi.end));
    if (bidi.right_to_left()) {
      std::ranges::reverse(segments);
    }
    for (auto const &part : segments) {
      auto run = shape_segment(*buffer, utf8, part, bidi, options);
      auto const metrics = part.face->metrics(run.size);
      result.ascent = std::max(result.ascent, metrics.ascent);
      result.descent = std::max(result.descent, metrics.descent);
      result.line_gap = std::max(result.line_gap, metrics.line_gap);
      auto const advance = run.advance;
      result.runs.push_back({std::make_shared<glyph_run const>(std::move(run)), x});
      x += advance;
    }
  }
  result.width = x;
  return result;
}

auto text_shaper::shape_spans(std::string_view utf8, shape_options const &options) const
    -> shaped_text {
  // The pieces are whole spans and the text between them, in logical order.
  struct piece {
    std::size_t begin;
    std::size_t end;
    bool bold;
    bool italic;
    float scale;
    float rise;
    bool capitals;
  };
  std::vector<piece> pieces;
  std::size_t at = 0;
  for (auto const &span : options.spans) {
    auto const begin = std::min(span.begin, utf8.size());
    auto const end = std::min(span.end, utf8.size());
    if (begin < at || end <= begin) {
      continue;
    }
    if (begin > at) {
      pieces.push_back({at, begin, false, false, 1, 0, false});
    }
    pieces.push_back({begin, end, span.bold, span.italic, span.scale, span.rise, span.capitals});
    at = end;
  }
  if (at < utf8.size() || pieces.empty()) {
    pieces.push_back({at, utf8.size(), false, false, 1, 0, false});
  }

  auto result = shaped_text{};
  auto const paragraph = resolve_bidi(utf8, options.direction);
  result.base_level = paragraph.base_level;
  auto base = options;
  base.spans.clear();
  base.direction =
      paragraph.base_level % 2 == 1 ? bidi_base::right_to_left : bidi_base::left_to_right;
  // In a right-to-left paragraph the first piece is at the right.
  if (paragraph.base_level % 2 == 1) {
    std::ranges::reverse(pieces);
  }
  auto x = 0.0F;
  for (auto const &part : pieces) {
    auto piece_options = base;
    if (part.bold) {
      piece_options.style.weight = font_weight::bold;
    }
    piece_options.italic = base.italic || part.italic;
    piece_options.style.font_size = base.style.font_size * part.scale;
    auto text = std::string{utf8.substr(part.begin, part.end - part.begin)};
    if (part.capitals && options.capital != nullptr) {
      text = capitalized(text, options.capital);
    }
    auto shaped = shape(text, piece_options);
    float const lift = part.rise * base.style.font_size;
    result.ascent = std::max(result.ascent, shaped.ascent + lift);
    result.descent = std::max(result.descent, shaped.descent - lift);
    result.line_gap = std::max(result.line_gap, shaped.line_gap);
    for (auto &placed : shaped.runs) {
      auto run = std::make_shared<glyph_run>(*placed.run);
      for (auto &g : run->glyphs) {
        g.cluster += static_cast<std::uint32_t>(part.begin);
        g.position.y -= lift;
      }
      run->text_begin += part.begin;
      run->text_end += part.begin;
      result.runs.push_back({std::move(run), x + placed.x});
    }
    x += shaped.width;
  }
  result.width = x;
  return result;
}

} // namespace basalt::text
