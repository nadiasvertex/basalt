#include "basalt/text/font_manager.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdlib>
#include <set>
#include <system_error>
#include <utility>

#include "font_face_impl.h"

namespace basalt::text {
namespace {

using blob_ptr = std::shared_ptr<hb_blob_t>;

auto lowercase(std::string_view text) -> std::string {
  auto result = std::string{text};
  std::ranges::transform(result, result.begin(),
                         [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return result;
}

auto name_string(hb_face_t *face, hb_ot_name_id_t id) -> std::string {
  auto const length = hb_ot_name_get_utf8(face, id, HB_LANGUAGE_INVALID, nullptr, nullptr);
  if (length == 0) {
    return {};
  }
  auto capacity = length + 1;
  auto text = std::string(capacity, '\0');
  hb_ot_name_get_utf8(face, id, HB_LANGUAGE_INVALID, &capacity, text.data());
  text.resize(capacity);
  return text;
}

auto first_name(hb_face_t *face, hb_ot_name_id_t preferred, hb_ot_name_id_t fallback)
    -> std::string {
  auto text = name_string(face, preferred);
  return text.empty() ? name_string(face, fallback) : text;
}

auto advance_of(hb_font_t *font, char32_t scalar) -> hb_position_t {
  auto glyph = hb_codepoint_t{0};
  if (hb_font_get_nominal_glyph(font, scalar, &glyph) == 0) {
    return 0;
  }
  return hb_font_get_glyph_h_advance(font, glyph);
}

auto next_face_id() -> std::uint64_t {
  static auto counter = std::atomic<std::uint64_t>{1};
  return counter.fetch_add(1);
}

// Builds the faces in `blob`. Faces without glyphs are not fonts and are
// skipped.
auto load_faces(blob_ptr const &blob, std::filesystem::path const &path,
                std::shared_ptr<std::vector<std::byte> const> const &memory)
    -> std::vector<font_face_ptr> {
  auto faces = std::vector<font_face_ptr>{};
  auto const count = hb_face_count(blob.get());
  for (auto index = 0U; index < count; ++index) {
    auto probe = hb_ptr<hb_face_t>{hb_face_create(blob.get(), index)};
    if (hb_face_get_glyph_count(probe.get()) == 0) {
      continue;
    }
    // A variable font with a weight axis becomes one face per common weight.
    auto weights = std::vector<int>{0};
    auto axis = hb_ot_var_axis_info_t{};
    if (hb_ot_var_find_axis_info(probe.get(), HB_OT_TAG_VAR_AXIS_WEIGHT, &axis) != 0) {
      weights.clear();
      for (auto const weight : {400, 600, 700}) {
        if (static_cast<float>(weight) >= axis.min_value &&
            static_cast<float>(weight) <= axis.max_value) {
          weights.push_back(weight);
        }
      }
      if (weights.empty()) {
        weights.push_back(0);
      }
    }
    for (auto const variation_weight : weights) {
      auto face = hb_ptr<hb_face_t>{hb_face_create(blob.get(), index)};
      auto state = std::make_unique<font_face::impl>();
      state->id = next_face_id();
      state->path = path;
      state->index = index;
      state->upem = std::max(1U, hb_face_get_upem(face.get()));
      state->blob = blob;
      state->memory = memory;
      state->lookup_font.reset(hb_font_create(face.get()));
      state->shape_font.reset(hb_font_create(face.get()));
      if (variation_weight != 0) {
        auto const variation =
            hb_variation_t{HB_OT_TAG_VAR_AXIS_WEIGHT, static_cast<float>(variation_weight)};
        hb_font_set_variations(state->lookup_font.get(), &variation, 1);
        hb_font_set_variations(state->shape_font.get(), &variation, 1);
        state->variation_weight = variation_weight;
      }
      state->family =
          first_name(face.get(), HB_OT_NAME_ID_TYPOGRAPHIC_FAMILY, HB_OT_NAME_ID_FONT_FAMILY);
      if (state->family.empty()) {
        // Fonts without a name table are still usable; the file name stands in.
        state->family = path.stem().string();
      }
      state->style =
          first_name(face.get(), HB_OT_NAME_ID_TYPOGRAPHIC_SUBFAMILY, HB_OT_NAME_ID_FONT_SUBFAMILY);
      state->weight = variation_weight != 0
                          ? variation_weight
                          : std::clamp(static_cast<int>(hb_style_get_value(state->lookup_font.get(),
                                                                           HB_STYLE_TAG_WEIGHT)),
                                       100, 900);
      state->italic =
          hb_style_get_value(state->lookup_font.get(), HB_STYLE_TAG_ITALIC) > 0.5F ||
          hb_style_get_value(state->lookup_font.get(), HB_STYLE_TAG_SLANT_ANGLE) != 0.0F;
      auto const narrow = advance_of(state->lookup_font.get(), U'i');
      state->fixed_pitch = narrow != 0 && narrow == advance_of(state->lookup_font.get(), U'W');
      state->face = std::move(face);
      faces.push_back(std::make_shared<font_face const>(std::move(state)));
    }
  }
  return faces;
}

auto is_font_file(std::filesystem::path const &path) -> bool {
  auto const extension = lowercase(path.extension().string());
  return extension == ".ttf" || extension == ".otf" || extension == ".ttc" || extension == ".otc";
}

auto environment(char const *name) -> std::filesystem::path {
  auto const *value = std::getenv(name);
  return value == nullptr ? std::filesystem::path{} : std::filesystem::path{value};
}

} // namespace

font_manager::font_manager() = default;
font_manager::~font_manager() = default;

auto font_manager::add_file(std::filesystem::path const &path) -> std::size_t {
  auto const text = path.string();
  auto *raw = hb_blob_create_from_file_or_fail(text.c_str());
  if (raw == nullptr) {
    return 0;
  }
  auto const blob = blob_ptr{raw, hb_deleter{}};
  auto loaded = load_faces(blob, path, nullptr);
  auto const added = loaded.size();
  std::ranges::move(loaded, std::back_inserter(faces_));
  fallback_cache_.clear();
  return added;
}

auto font_manager::add_data(std::shared_ptr<std::vector<std::byte> const> data) -> std::size_t {
  if (data == nullptr || data->empty()) {
    return 0;
  }
  auto *raw = hb_blob_create(reinterpret_cast<char const *>(data->data()),
                             static_cast<unsigned>(data->size()), HB_MEMORY_MODE_READONLY, nullptr,
                             nullptr);
  auto const blob = blob_ptr{raw, hb_deleter{}};
  auto loaded = load_faces(blob, {}, data);
  auto const added = loaded.size();
  std::ranges::move(loaded, std::back_inserter(faces_));
  fallback_cache_.clear();
  return added;
}

auto font_manager::add_directory(std::filesystem::path const &directory, bool recursive)
    -> std::size_t {
  auto added = std::size_t{0};
  auto error = std::error_code{};
  auto const visit = [&](std::filesystem::directory_entry const &entry) {
    auto status_error = std::error_code{};
    if (entry.is_regular_file(status_error) && is_font_file(entry.path())) {
      added += add_file(entry.path());
    }
  };
  if (recursive) {
    for (auto it =
             std::filesystem::recursive_directory_iterator{
                 directory, std::filesystem::directory_options::skip_permission_denied, error};
         !error && it != std::filesystem::recursive_directory_iterator{}; it.increment(error)) {
      visit(*it);
    }
  } else {
    for (auto it =
             std::filesystem::directory_iterator{
                 directory, std::filesystem::directory_options::skip_permission_denied, error};
         !error && it != std::filesystem::directory_iterator{}; it.increment(error)) {
      visit(*it);
    }
  }
  return added;
}

auto font_manager::system_font_directories() -> std::vector<std::filesystem::path> {
  auto directories = std::vector<std::filesystem::path>{};
#if defined(__APPLE__)
  directories = {"/System/Library/Fonts", "/Library/Fonts"};
  if (auto const home = environment("HOME"); !home.empty()) {
    directories.push_back(home / "Library/Fonts");
  }
#elif defined(_WIN32)
  if (auto const windows = environment("WINDIR"); !windows.empty()) {
    directories.push_back(windows / "Fonts");
  }
  if (auto const local = environment("LOCALAPPDATA"); !local.empty()) {
    directories.push_back(local / "Microsoft/Windows/Fonts");
  }
#else
  directories = {"/usr/share/fonts", "/usr/local/share/fonts"};
  if (auto const home = environment("HOME"); !home.empty()) {
    directories.push_back(home / ".local/share/fonts");
    directories.push_back(home / ".fonts");
  }
#endif
  return directories;
}

auto font_manager::add_system_fonts() -> std::size_t {
  auto added = std::size_t{0};
  for (auto const &directory : system_font_directories()) {
    added += add_directory(directory);
  }
  return added;
}

auto font_manager::families() const -> std::vector<std::string> {
  auto names = std::set<std::string>{};
  for (auto const &face : faces_) {
    names.insert(face->family());
  }
  return {names.begin(), names.end()};
}

auto font_manager::match(font_request const &request) const -> font_face_ptr {
  auto const wanted = lowercase(request.family);
  auto best = font_face_ptr{};
  auto best_cost = std::pair{0, 0};
  for (auto const &face : faces_) {
    if (lowercase(face->family()) != wanted) {
      continue;
    }
    auto const cost = std::pair{face->italic() == request.italic ? 0 : 1,
                                std::abs(face->weight() - request.weight)};
    if (best == nullptr || cost < best_cost) {
      best = face;
      best_cost = cost;
    }
  }
  return best;
}

auto font_manager::fixed_pitch_face(font_request const &request) const -> font_face_ptr {
  static constexpr std::array preferred = {
      "menlo",           "sf mono",     "consolas", "dejavu sans mono",
      "liberation mono", "courier new", "courier"};
  for (auto const *family : preferred) {
    if (auto face = match({family, request.weight, request.italic}); face && face->fixed_pitch()) {
      return face;
    }
  }
  auto best = font_face_ptr{};
  auto best_cost = std::pair{0, 0};
  for (auto const &face : faces_) {
    if (!face->fixed_pitch() || !face->covers(U'A') || face->family().starts_with('.')) {
      continue;
    }
    auto const cost = std::pair{face->italic() == request.italic ? 0 : 1,
                                std::abs(face->weight() - request.weight)};
    if (best == nullptr || cost < best_cost) {
      best = face;
      best_cost = cost;
    }
  }
  return best;
}

auto font_manager::primary_face(font_request const &request) const -> font_face_ptr {
  if (auto face = match(request)) {
    return face;
  }
  if (lowercase(request.family).find("mono") != std::string::npos) {
    if (auto face = fixed_pitch_face(request)) {
      return face;
    }
  }
  if (auto face = match({"Noto Sans", request.weight, request.italic})) {
    return face;
  }
  for (auto const &face : faces_) {
    if (face->covers(U'A') && !face->family().starts_with('.')) {
      return face;
    }
  }
  return faces_.empty() ? nullptr : faces_.front();
}

auto font_manager::rank(font_face const &face, font_request const &request,
                        bool prefer_fixed_pitch) const -> std::tuple<int, int, int> {
  auto const family = lowercase(face.family());
  auto tier = 5;
  if (family == lowercase(request.family)) {
    tier = 0;
  } else if (prefer_fixed_pitch && (family == "noto sans mono" || face.fixed_pitch())) {
    tier = 1;
  } else if (family == "noto sans") {
    tier = 2;
  } else if (family.starts_with("noto sans ")) {
    tier = 3;
  } else if (family.starts_with("noto ")) {
    tier = 4;
  } else if (family.starts_with('.')) {
    tier = 6;
  }
  return {tier, face.italic() == request.italic ? 0 : 1, std::abs(face.weight() - request.weight)};
}

auto font_manager::fallback_for(char32_t scalar, font_request const &request) -> font_face_ptr {
  return fallback_for(std::u32string_view{&scalar, 1}, request);
}

auto font_manager::fallback_for(std::u32string_view scalars, font_request const &request)
    -> font_face_ptr {
  auto key =
      cache_key{std::u32string{scalars}, lowercase(request.family), request.weight, request.italic};
  if (auto const hit = fallback_cache_.find(key); hit != fallback_cache_.end()) {
    return hit->second;
  }
  auto const primary = primary_face(request);
  auto const fixed = primary != nullptr && primary->fixed_pitch();
  auto best = font_face_ptr{};
  auto best_rank = std::tuple<int, int, int>{};
  for (auto const &face : faces_) {
    if (!std::ranges::all_of(scalars, [&](char32_t scalar) { return face->covers(scalar); })) {
      continue;
    }
    auto const candidate = rank(*face, request, fixed);
    if (best == nullptr || candidate < best_rank) {
      best = face;
      best_rank = candidate;
    }
  }
  fallback_cache_.emplace(std::move(key), best);
  return best;
}

} // namespace basalt::text
