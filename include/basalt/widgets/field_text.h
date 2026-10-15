#pragma once

#include "basalt/widgets/control_size.h"
#include "basalt/widgets/text_line.h"

#include <string>

namespace basalt {

// The texts that surround a form control's value: a label, helper text, a
// placeholder and the error flag, with the size that picks the label role.
// Setters return whether anything changed so the owner can invalidate.
class field_text {
public:
  explicit field_text(std::string label = {}, control_size size = control_size::medium);

  [[nodiscard]] auto label() const -> text_line const & { return label_; }
  [[nodiscard]] auto helper() const -> text_line const & { return helper_; }
  [[nodiscard]] auto placeholder() const -> text_line const & { return placeholder_; }

  auto set_label(std::string text) -> bool { return label_.set_text(std::move(text)); }
  auto set_helper(std::string text) -> bool { return helper_.set_text(std::move(text)); }
  auto set_placeholder(std::string text) -> bool { return placeholder_.set_text(std::move(text)); }

  [[nodiscard]] auto has_error() const -> bool { return error_; }
  auto set_error(bool error) -> bool;

  [[nodiscard]] auto size() const -> control_size { return size_; }
  auto set_size(control_size size) -> bool;

private:
  text_line label_;
  text_line helper_;
  text_line placeholder_;
  control_size size_;
  bool error_ = false;
};

} // namespace basalt
