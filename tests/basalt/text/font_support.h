#pragma once

#include "basalt/text/font_manager.h"

#include <filesystem>

namespace med::test {

// The machine's fonts, scanned once per process. Tests skip when a script they
// need is missing, since font sets differ between machines.
inline auto system_fonts() -> basalt::text::font_manager & {
  static auto manager = [] {
    auto fonts = std::make_unique<basalt::text::font_manager>();
    fonts->add_system_fonts();
    return fonts;
  }();
  return *manager;
}

inline auto temp_directory() -> std::filesystem::path {
  auto dir = std::filesystem::temp_directory_path() / "med_tests";
  std::filesystem::create_directories(dir);
  return dir;
}

inline auto has_coverage(basalt::text::font_manager &fonts, char32_t scalar) -> bool {
  return fonts.fallback_for(scalar, {"Noto Sans"}) != nullptr;
}

} // namespace med::test
