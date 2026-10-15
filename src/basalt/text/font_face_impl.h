#pragma once

#include "basalt/text/font_face.h"

#include <hb-ot.h>
#include <hb.h>

#include <memory>
#include <vector>

namespace basalt::text {

struct hb_deleter {
  auto operator()(hb_blob_t *blob) const -> void { hb_blob_destroy(blob); }
  auto operator()(hb_face_t *face) const -> void { hb_face_destroy(face); }
  auto operator()(hb_font_t *font) const -> void { hb_font_destroy(font); }
};

template <class T> using hb_ptr = std::unique_ptr<T, hb_deleter>;

struct font_face::impl {
  std::uint64_t id = 0;
  std::string family;
  std::string style;
  int weight = 400;
  // The weight-axis value applied to a variable font, or zero for a static face.
  int variation_weight = 0;
  bool italic = false;
  bool fixed_pitch = false;
  std::filesystem::path path;
  unsigned index = 0;
  unsigned upem = 1000;

  // Declared in destruction order: the font goes first, the blob last.
  std::shared_ptr<hb_blob_t> blob;
  std::shared_ptr<std::vector<std::byte> const> memory;
  hb_ptr<hb_face_t> face;
  // Unscaled (one unit per font unit) for glyph and metric lookups.
  hb_ptr<hb_font_t> lookup_font;
  // Rescaled for every shaping call.
  hb_ptr<hb_font_t> shape_font;
};

[[nodiscard]] auto native_impl(font_face const &face) -> font_face::impl &;

} // namespace basalt::text
