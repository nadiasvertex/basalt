#pragma once

#include "basalt/text/font_face.h"

#include <cstddef>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace basalt::text {

// What a caller asks for. Weight is CSS-style, 100 to 900.
struct font_request {
  std::string family;
  int weight = 400;
  bool italic = false;
};

// Finds fonts and chooses fallbacks. Fonts come from directories, files or
// memory; nothing is discovered until a `add_` function is called, so tests
// and applications control exactly which fonts exist.
//
// Fallback prefers the requested family, then Noto Sans Mono for fixed-pitch
// requests, Noto Sans, the other Noto families, and last every remaining font.
// Not thread-safe.
class font_manager {
public:
  font_manager();
  font_manager(font_manager const &) = delete;
  auto operator=(font_manager const &) -> font_manager & = delete;
  ~font_manager();

  // Each returns the number of faces added. Files that are not fonts are
  // skipped without error.
  auto add_file(std::filesystem::path const &path) -> std::size_t;
  auto add_data(std::shared_ptr<std::vector<std::byte> const> data) -> std::size_t;
  auto add_directory(std::filesystem::path const &directory, bool recursive = true) -> std::size_t;
  // Adds the operating system's font directories.
  auto add_system_fonts() -> std::size_t;

  [[nodiscard]] static auto system_font_directories() -> std::vector<std::filesystem::path>;

  [[nodiscard]] auto faces() const -> std::span<font_face_ptr const> { return faces_; }
  // Sorted family names without duplicates.
  [[nodiscard]] auto families() const -> std::vector<std::string>;

  // The closest style of the requested family, or null if the family is absent.
  // Family names compare case-insensitively.
  [[nodiscard]] auto match(font_request const &request) const -> font_face_ptr;

  // The best face that covers `scalar`, or null if no font does. The result is
  // cached until fonts are added.
  [[nodiscard]] auto fallback_for(char32_t scalar, font_request const &request) -> font_face_ptr;
  // The best face that covers every scalar, for a base character and its marks.
  [[nodiscard]] auto fallback_for(std::u32string_view scalars, font_request const &request)
      -> font_face_ptr;

  // The face used for text in `request` when it has no better candidate: the
  // matched family, else Noto Sans (or Mono), else the first face.
  [[nodiscard]] auto primary_face(font_request const &request) const -> font_face_ptr;

private:
  using cache_key = std::tuple<std::u32string, std::string, int, bool>;

  // The best installed fixed-pitch face, or null if none exists.
  [[nodiscard]] auto fixed_pitch_face(font_request const &request) const -> font_face_ptr;
  [[nodiscard]] auto rank(font_face const &face, font_request const &request,
                          bool prefer_fixed_pitch) const -> std::tuple<int, int, int>;

  std::vector<font_face_ptr> faces_;
  std::map<cache_key, font_face_ptr> fallback_cache_;
};

} // namespace basalt::text
