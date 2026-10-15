#include "skia_replay.h"

#include "basalt/text/font_face.h"

#include <cstdint>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkData.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontArguments.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkImage.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkRect.h"
#include "include/core/SkSamplingOptions.h"
#include "include/core/SkStream.h"
#include "include/core/SkSurface.h"
#include "include/core/SkTextBlob.h"

#if defined(__APPLE__)
#include "include/ports/SkFontMgr_mac_ct.h"
#elif defined(_WIN32)
#include "include/ports/SkTypeface_win.h"
#else
#include "include/ports/SkFontMgr_fontconfig.h"
#endif

namespace example {
namespace {

auto to_sk_color(basalt::rgba color) -> SkColor {
  return SkColorSetARGB(color.a, color.r, color.g, color.b);
}

auto to_sk_rect(basalt::rect bounds) -> SkRect {
  return SkRect::MakeLTRB(bounds.left(), bounds.top(), bounds.right(), bounds.bottom());
}

auto to_sk_matrix(basalt::affine_transform const &transform) -> SkMatrix {
  return SkMatrix::MakeAll(transform.xx, transform.xy, transform.tx, transform.yx, transform.yy,
                           transform.ty, 0, 0, 1);
}

auto make_paint(basalt::rgba color, SkPaint::Style style, float width) -> SkPaint {
  auto paint = SkPaint{};
  paint.setAntiAlias(true);
  paint.setColor(to_sk_color(color));
  paint.setStyle(style);
  paint.setStrokeWidth(width);
  return paint;
}

// Typefaces are made from the face's own font file, so glyph ids from HarfBuzz mean the same thing
// to Skia. The manager never enumerates fonts.
auto font_manager() -> sk_sp<SkFontMgr> const & {
  static auto const manager = [] {
#if defined(__APPLE__)
    return SkFontMgr_New_CoreText(nullptr);
#elif defined(_WIN32)
    return SkFontMgr_New_DirectWrite();
#else
    return SkFontMgr_New_FontConfig(nullptr);
#endif
  }();
  return manager;
}

auto make_typeface(basalt::text::font_face_ptr const &face) -> sk_sp<SkTypeface> {
  auto const bytes = face->data();
  // The data object owns a reference to the face, which keeps the font file alive while Skia
  // reads it.
  auto owner = std::make_unique<basalt::text::font_face_ptr>(face);
  auto data = SkData::MakeWithProc(
      bytes.data(), bytes.size(),
      [](void const *, void *context) {
        std::unique_ptr<basalt::text::font_face_ptr>{
            static_cast<basalt::text::font_face_ptr *>(context)}
            .reset();
      },
      owner.release());
  if (face->variation_weight() == 0) {
    return font_manager()->makeFromData(std::move(data), static_cast<int>(face->face_index()));
  }
  auto const coordinate = SkFontArguments::VariationPosition::Coordinate{
      SkSetFourByteTag('w', 'g', 'h', 't'), static_cast<float>(face->variation_weight())};
  auto arguments = SkFontArguments{};
  arguments.setCollectionIndex(static_cast<int>(face->face_index()));
  arguments.setVariationDesignPosition({&coordinate, 1});
  return font_manager()->makeFromStream(SkMemoryStream::Make(std::move(data)), arguments);
}

using typeface_cache = std::unordered_map<std::uint64_t, sk_sp<SkTypeface>>;
using image_cache = std::unordered_map<std::uint64_t, sk_sp<SkImage>>;

auto typeface_for(typeface_cache &typefaces, basalt::text::font_face_ptr const &face)
    -> sk_sp<SkTypeface> {
  auto const found = typefaces.find(face->id());
  if (found != typefaces.end()) {
    return found->second;
  }
  return typefaces.emplace(face->id(), make_typeface(face)).first->second;
}

auto image_for(image_cache &images, basalt::raster_image const &source) -> sk_sp<SkImage> {
  if (auto const found = images.find(source.id()); found != images.end()) {
    return found->second;
  }
  auto const info =
      SkImageInfo::Make(static_cast<int>(source.width()), static_cast<int>(source.height()),
                        kRGBA_8888_SkColorType, kUnpremul_SkAlphaType);
  auto const pixmap = SkPixmap{info, source.pixels().data(), std::size_t{source.width()} * 4};
  return images.emplace(source.id(), SkImages::RasterFromPixmapCopy(pixmap)).first->second;
}

auto draw_glyph_run(SkCanvas &canvas, typeface_cache &typefaces,
                    basalt::draw_glyph_run_command const &command) -> void {
  auto const &run = command.run;
  if (run == nullptr || run->face == nullptr || run->glyphs.empty()) {
    return;
  }
  auto typeface = typeface_for(typefaces, run->face);
  if (typeface == nullptr) {
    return;
  }
  auto font = SkFont{std::move(typeface), run->size};
  font.setSubpixel(true);
  font.setEdging(SkFont::Edging::kAntiAlias);
  font.setHinting(SkFontHinting::kNone);

  auto builder = SkTextBlobBuilder{};
  auto const &buffer = builder.allocRunPos(font, static_cast<int>(run->glyphs.size()));
  for (auto i = std::size_t{0}; i < run->glyphs.size(); ++i) {
    auto const &glyph = run->glyphs[i];
    buffer.glyphs[i] = static_cast<SkGlyphID>(glyph.id);
    buffer.pos[2 * i] = glyph.position.x;
    buffer.pos[2 * i + 1] = glyph.position.y;
  }
  auto paint = SkPaint{};
  paint.setAntiAlias(true);
  paint.setColor(to_sk_color(command.color));
  canvas.drawTextBlob(builder.make(), command.origin.x, command.origin.y, paint);
}

auto draw_image(SkCanvas &canvas, image_cache &images, basalt::draw_image_command const &command)
    -> void {
  if (command.image == nullptr) {
    return;
  }
  if (auto const image = image_for(images, *command.image); image != nullptr) {
    auto const options = SkSamplingOptions{SkFilterMode::kLinear, SkMipmapMode::kLinear};
    canvas.drawImageRect(image, to_sk_rect(command.bounds), options);
  }
}

// Clips, transforms and layers share the canvas save stack, so every pop is a restore.
auto replay_commands(SkCanvas &canvas, typeface_cache &typefaces, image_cache &images,
            basalt::paint_commands const &commands) -> void {
  auto const base = canvas.getSaveCount();
  for (auto const &command : commands) {
    std::visit(
        [&](auto const &value) {
          using type = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<type, basalt::fill_rect_command>) {
            canvas.drawRect(to_sk_rect(value.bounds),
                            make_paint(value.color, SkPaint::kFill_Style, 0));
          } else if constexpr (std::is_same_v<type, basalt::stroke_rect_command>) {
            canvas.drawRect(to_sk_rect(value.bounds),
                            make_paint(value.color, SkPaint::kStroke_Style, value.width));
          } else if constexpr (std::is_same_v<type, basalt::fill_rounded_rect_command>) {
            canvas.drawRoundRect(to_sk_rect(value.bounds), value.radius, value.radius,
                                 make_paint(value.color, SkPaint::kFill_Style, 0));
          } else if constexpr (std::is_same_v<type, basalt::stroke_rounded_rect_command>) {
            canvas.drawRoundRect(to_sk_rect(value.bounds), value.radius, value.radius,
                                 make_paint(value.color, SkPaint::kStroke_Style, value.width));
          } else if constexpr (std::is_same_v<type, basalt::line_command>) {
            canvas.drawLine(value.from.x, value.from.y, value.to.x, value.to.y,
                            make_paint(value.color, SkPaint::kStroke_Style, value.width));
          } else if constexpr (std::is_same_v<type, basalt::draw_glyph_run_command>) {
            draw_glyph_run(canvas, typefaces, value);
          } else if constexpr (std::is_same_v<type, basalt::draw_image_command>) {
            draw_image(canvas, images, value);
          } else if constexpr (std::is_same_v<type, basalt::push_clip_command>) {
            canvas.save();
            canvas.clipRect(to_sk_rect(value.bounds), SkClipOp::kIntersect, false);
          } else if constexpr (std::is_same_v<type, basalt::push_transform_command>) {
            canvas.save();
            canvas.concat(to_sk_matrix(value.transform));
          } else if constexpr (std::is_same_v<type, basalt::begin_layer_command>) {
            canvas.saveLayerAlpha(nullptr, value.opacity);
          } else {
            static_assert(std::is_same_v<type, basalt::pop_clip_command> ||
                          std::is_same_v<type, basalt::pop_transform_command> ||
                          std::is_same_v<type, basalt::end_layer_command>);
            if (canvas.getSaveCount() > base) {
              canvas.restore();
            }
          }
        },
        command);
  }
  canvas.restoreToCount(base);
}

} // namespace

struct skia_replayer::impl {
  typeface_cache typefaces;
  image_cache images;
};

skia_replayer::skia_replayer() : impl_(std::make_unique<impl>()) {}
skia_replayer::~skia_replayer() = default;

auto skia_replayer::replay(SkCanvas &canvas, basalt::paint_commands const &commands) -> void {
  replay_commands(canvas, impl_->typefaces, impl_->images, commands);
}

} // namespace example
