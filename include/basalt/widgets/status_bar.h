#pragma once

#include "basalt/widgets/box.h"
#include "basalt/widgets/label.h"

#include <string>
#include <unordered_map>

namespace basalt {

// A row of text sections along the bottom of a window: a message, the caret
// position, the encoding and similar facts. Sections keep the order they were
// added in. A section with `grow` above zero takes the spare width, which
// pushes the sections added after it to the right edge.
//
// The bar is a polite live region, so assistive technology announces changes
// to its sections without moving focus.
class status_bar : public box {
public:
  explicit status_bar(std::string name = "Status");

  // Adds a section named `id` and returns its label.
  auto add_section(std::string id, std::string text = {}, float grow = 0) -> label &;
  // Null when there is no section with `id`.
  [[nodiscard]] auto section(std::string const &id) const -> label *;
  // False when there is no section with `id`.
  auto set_section_text(std::string const &id, std::string text) -> bool;

  [[nodiscard]] auto name() const -> std::string const & { return name_; }
  auto set_name(std::string name) -> void { name_ = std::move(name); }

  [[nodiscard]] auto accessibility_info() const -> accessibility::info override;

protected:
  auto on_paint(render_context &context) const -> void override;

private:
  std::string name_;
  std::unordered_map<std::string, widget_id> sections_;
};

} // namespace basalt
