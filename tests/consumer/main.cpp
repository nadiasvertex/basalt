#include "basalt/basalt.h"

#include <print>

// Shapes one line with the font named on the command line. Exits non-zero unless shaping
// produced one glyph per letter, which needs the HarfBuzz that is built into Basalt.
auto main(int argc, char **argv) -> int {
  if (argc != 2) {
    std::println(stderr, "usage: consumer <font file>");
    return 2;
  }
  auto fonts = basalt::text::font_manager{};
  if (fonts.add_file(argv[1]) == 0) {
    std::println(stderr, "consumer: cannot load {}", argv[1]);
    return 1;
  }
  auto options = basalt::text::shape_options{};
  options.style = basalt::text_style{"Noto Sans", 20};
  auto const shaped = basalt::text::text_shaper{fonts}.shape("Basalt", options);
  auto glyphs = std::size_t{0};
  for (auto const &run : shaped.runs) {
    glyphs += run.run->glyphs.size();
  }
  std::println("shaped {} glyphs, {} wide", glyphs, shaped.width);
  return glyphs == 6 && shaped.width > 0 ? 0 : 1;
}
