#include "basalt/widgets/progress_bar.h"

#include "basalt/render_context.h"
#include "basalt/widget_tree.h"

#include <algorithm>
#include <cmath>

namespace basalt {

namespace {

constexpr length track_width = fixed(120);
constexpr length track_height = fixed(6);
// The share of the track the indeterminate segment covers.
constexpr float segment_share = 0.3f;

} // namespace

progress_bar::progress_bar(std::optional<float> fraction) { set_fraction(fraction); }

auto progress_bar::set_fraction(std::optional<float> fraction) -> void {
  if (fraction) {
    fraction = std::clamp(*fraction, 0.0f, 1.0f);
  }
  if (fraction != fraction_) {
    fraction_ = fraction;
    invalidate_paint();
  }
}

auto progress_bar::tick(float step) -> void {
  if (!fraction_) {
    phase_ = std::fmod(phase_ + step, 1.0f);
  }
  invalidate_paint();
}

auto progress_bar::accessibility_info() const -> accessibility::info {
  accessibility::info details;
  details.kind = accessibility::role::progress_bar;
  details.name = "Progress";
  details.busy = !fraction_ || *fraction_ < 1.0f;
  if (fraction_) {
    details.range = accessibility::value_range{0, 100, 1};
    details.numeric_value = std::round(*fraction_ * 100.0f);
  }
  return details;
}

auto progress_bar::on_measure(constraints const &) const -> basalt::size {
  return {resolve(track_width), resolve(track_height)};
}

auto progress_bar::on_paint(render_context &context) const -> void {
  if (tree() == nullptr) {
    return;
  }
  auto const &colors = tree()->theme().colors;
  basalt::size const extent = bounds().extent;
  float const radius = extent.height / 2;
  context.fill_rounded_rect({{}, extent}, radius, colors.background_tertiary);
  if (fraction_) {
    float const filled = extent.width * *fraction_;
    if (filled > 0) {
      context.fill_rounded_rect({{}, {std::max(filled, extent.height), extent.height}}, radius,
                                colors.border_accent);
    }
    return;
  }
  // The segment runs off the right end and comes back in at the left.
  float const length = extent.width * segment_share;
  float const left = phase_ * (extent.width + length) - length;
  float const from = std::max(left, 0.0f);
  float const to = std::min(left + length, extent.width);
  if (to > from) {
    context.fill_rounded_rect({{from, 0}, {to - from, extent.height}}, radius,
                              colors.border_accent);
  }
}

} // namespace basalt
