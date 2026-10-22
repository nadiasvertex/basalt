#pragma once

#include "basalt/layout.h"
#include "basalt/widget.h"
#include "basalt/widgets/menu.h"
#include "basalt/widgets/scroll_view.h"

#include <algorithm>
#include <memory>
#include <utility>

namespace basalt {

// The list shown in the popup: a menu in a scroll view that is as wide as the
// field and as tall as its rows, up to a limit.
class option_list : public widget {
public:
  option_list(float min_width, float max_height) : min_width_(min_width), max_height_(max_height) {
    set_hit_testable(false);
  }

  auto set_list(std::unique_ptr<menu> list) -> menu & {
    menu &added = *list;
    scroller_ = &emplace_child<scroll_view>(scroll_axes::vertical);
    scroller_->set_content(std::move(list));
    scroller_->set_scrollbars(true);
    return added;
  }

  [[nodiscard]] auto accessibility_info() const -> accessibility::info override {
    accessibility::info details;
    details.kind = accessibility::role::list_box;
    return details;
  }

protected:
  [[nodiscard]] auto on_measure(constraints const &limits) const -> basalt::size override {
    basalt::size natural;
    if (scroller_ != nullptr && scroller_->content() != nullptr) {
      natural = scroller_->content()->measure(constraints::loose({limits.max.width, unbounded}));
    }
    return {std::max(natural.width, min_width_), std::min(natural.height, max_height_)};
  }

  auto on_layout() -> void override {
    if (scroller_ != nullptr) {
      scroller_->set_bounds({{}, bounds().extent});
    }
  }

private:
  float min_width_;
  float max_height_;
  scroll_view *scroller_ = nullptr;
};

} // namespace basalt
