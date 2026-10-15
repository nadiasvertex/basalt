#pragma once

#include <functional>
#include <string>
#include <vector>

namespace basalt {

// What a widget asks the application's file dialog for.
struct file_dialog_request {
  bool multiple = false;
  // Suffixes such as ".png"; empty accepts any file.
  std::vector<std::string> extensions;
};

// The application's file chooser. Basalt never opens windows, so a widget
// that needs files calls `open` and the application shows its dialog. The
// application calls `done` with the chosen UTF-8 paths, or with none when the
// user cancels, and may do so after `open` returns.
struct file_dialog {
  using completion = std::function<void(std::vector<std::string>)>;

  std::function<void(file_dialog_request const &, completion)> open;

  [[nodiscard]] explicit operator bool() const { return static_cast<bool>(open); }
};

} // namespace basalt
