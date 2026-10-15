#pragma once

#include "basalt/text/font_manager.h"

#include <array>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace basalt::test {

// Noto Sans regular and italic, shipped in tests/data/fonts. Tests that only need Latin text use
// these, so they give the same results on every machine and never skip. A missing file is an
// error, not a reason to skip.
inline auto bundled_fonts() -> basalt::text::font_manager & {
  static auto manager = [] {
    constexpr auto files = std::array<std::string_view, 2>{
        "NotoSans-VariableFont_wdth,wght.ttf",
        "NotoSans-Italic-VariableFont_wdth,wght.ttf",
    };
    auto fonts = std::make_unique<basalt::text::font_manager>();
    for (auto const file : files) {
      auto const path = std::filesystem::path{BASALT_TEST_FONT_DIR} / file;
      if (fonts->add_file(path) == 0) {
        throw std::runtime_error("bundled test font is missing or unreadable: " + path.string());
      }
    }
    return fonts;
  }();
  return *manager;
}

// The machine's fonts, scanned once per process. Tests of font discovery and of scripts the
// bundled fonts lack use these, and skip when a script they need is missing.
inline auto system_fonts() -> basalt::text::font_manager & {
  static auto manager = [] {
    auto fonts = std::make_unique<basalt::text::font_manager>();
    fonts->add_system_fonts();
    return fonts;
  }();
  return *manager;
}

inline auto temp_directory() -> std::filesystem::path {
  auto dir = std::filesystem::temp_directory_path() / "basalt_tests";
  std::filesystem::create_directories(dir);
  return dir;
}

inline auto has_coverage(basalt::text::font_manager &fonts, char32_t scalar) -> bool {
  return fonts.fallback_for(scalar, {"Noto Sans"}) != nullptr;
}

} // namespace basalt::test
