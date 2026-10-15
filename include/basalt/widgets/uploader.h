#pragma once

#include "basalt/widget.h"
#include "basalt/widgets/button.h"
#include "basalt/widgets/chip.h"
#include "basalt/widgets/text_line.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace basalt {

enum class uploader_layout : std::uint8_t {
  // Only the Select Files button; the widget accepts no drops.
  no_drop_zone,
  // A single-row drop zone with the prompt and the button side by side.
  compact,
  // A taller drop zone with the prompt above the button.
  spacious,
};

enum class uploader_behavior : std::uint8_t {
  // Files are handed over as soon as they are chosen and show a busy chip
  // until `finish` is called for them.
  immediate,
  // One file is held until the application asks for it; choosing another
  // replaces it.
  deferred_single,
  // Files are held until the application asks for them.
  deferred_multiple,
};

// Collects files from a file dialog or from files dropped on the widget and
// lists them as removable chips. Files are held as paths; reading and
// sending them belongs to the application.
//
// The Select Files button calls the tree's `file_dialog`, which the
// application supplies. Without one, the button does nothing. A file that
// does not match the accepted suffixes is rejected and shows an error message
// below the chips. While files are dragged over the drop zone it shows the
// drag state.
class uploader : public widget {
public:
  explicit uploader(std::string name = {}, uploader_layout layout = uploader_layout::spacious,
                    uploader_behavior behavior = uploader_behavior::deferred_multiple);
  ~uploader() override;

  [[nodiscard]] auto name() const -> std::string const & { return name_; }
  auto set_name(std::string name) -> void { name_ = std::move(name); }
  [[nodiscard]] auto layout() const -> uploader_layout { return layout_; }
  auto set_layout(uploader_layout layout) -> void;
  [[nodiscard]] auto behavior() const -> uploader_behavior { return behavior_; }
  auto set_behavior(uploader_behavior behavior) -> void;

  [[nodiscard]] auto prompt() const -> std::string const & { return prompt_.text(); }
  auto set_prompt(std::string text) -> void;
  auto set_button_label(std::string label) -> void;
  // Suffixes such as ".png", compared without regard to case. Empty accepts
  // every file.
  auto set_accepted_suffixes(std::vector<std::string> suffixes) -> void;

  // Paths held now, in the order they were added.
  [[nodiscard]] auto files() const -> std::vector<std::string>;
  // Adds files as if they were chosen or dropped. Rejected files set the error
  // message. Returns the paths that were taken.
  auto add_files(std::vector<std::string> paths) -> std::vector<std::string>;
  auto clear() -> void;
  // The immediate behavior: the upload of `path` ended. A failure marks its
  // chip as an error.
  auto finish(std::string_view path, bool succeeded) -> void;

  // Shown below the chips in the error state. Empty clears the error.
  [[nodiscard]] auto error_message() const -> std::string const & { return error_.text(); }
  auto set_error_message(std::string message) -> void;
  [[nodiscard]] auto dragging() const -> bool { return dragging_; }

  // Called with the paths that were taken, after they are listed.
  auto set_on_added(std::function<void(std::vector<std::string> const &)> handler) -> void {
    on_added_ = std::move(handler);
  }
  // Called with the path of a file the user removed.
  auto set_on_removed(std::function<void(std::string const &)> handler) -> void {
    on_removed_ = std::move(handler);
  }

  // Opens the file dialog. False when there is none or the widget is disabled.
  auto select_files() -> bool;

  [[nodiscard]] auto accessibility_info() const -> accessibility::info override;

protected:
  auto on_event(event_context &context, event const &e) -> bool override;
  auto on_layout() -> void override;
  auto on_paint(render_context &context) const -> void override;
  [[nodiscard]] auto on_measure(constraints const &limits) const -> basalt::size override;

private:
  struct entry {
    std::uint64_t key = 0;
    std::string path;
    chip *shown = nullptr;
  };
  struct plan;

  [[nodiscard]] auto arrange(float width) const -> plan;
  [[nodiscard]] auto accepts(std::string_view path) const -> bool;
  auto removed(std::uint64_t key) -> void;

  std::string name_;
  uploader_layout layout_;
  uploader_behavior behavior_;
  text_line prompt_;
  text_line error_;
  std::vector<std::string> suffixes_;
  std::vector<entry> entries_;
  std::uint64_t next_key_ = 1;
  bool dragging_ = false;
  button *select_ = nullptr;
  chip_group *list_ = nullptr;
  std::function<void(std::vector<std::string> const &)> on_added_;
  std::function<void(std::string const &)> on_removed_;
  std::shared_ptr<uploader *> alive_;
};

} // namespace basalt
