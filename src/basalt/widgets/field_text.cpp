#include "basalt/widgets/field_text.h"

#include <utility>

namespace basalt {

namespace {

constexpr text_role helper_role = text_role::small;

} // namespace

field_text::field_text(std::string label, control_size size)
    : label_(std::move(label), label_role_for(size)), helper_({}, helper_role),
      placeholder_({}, label_role_for(size)), size_(size) {}

auto field_text::set_error(bool error) -> bool {
  if (error == error_) {
    return false;
  }
  error_ = error;
  return true;
}

auto field_text::set_size(control_size size) -> bool {
  if (size == size_) {
    return false;
  }
  size_ = size;
  label_.set_role(label_role_for(size));
  placeholder_.set_role(label_role_for(size));
  return true;
}

} // namespace basalt
