#pragma once

#include "basalt/accessibility/node.h"
#include "basalt/layout.h"
#include "basalt/text/paragraph.h"
#include "basalt/widget.h"
#include "basalt/widgets/text_line.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt {

// The system clipboard as the application provides it. Both functions must
// be set for the clipboard shortcuts to work.
struct text_clipboard {
  std::function<std::string()> read;
  std::function<void(std::string)> write;
};

// An editable text control for dialogs, search boxes and other short input,
// in a single-line or a multi-line form.
//
// Text is UTF-8 and all offsets are UTF-8 byte offsets on grapheme
// boundaries. The selection is an `anchor` (where it started) and a `focus`
// (the caret). The caret moves by grapheme, and by word with Alt or Ctrl;
// Cmd/Meta with Left or Right goes to the line start or end. Shift extends
// the selection. Ctrl or Meta with A, C, X and V select all, copy, cut and
// paste through the application's `text_clipboard`.
//
// A single-line field never wraps, turns line breaks in inserted text into
// spaces, scrolls horizontally to keep the caret visible and calls the submit
// handler on Enter. A multi-line field wraps at its width, grows with its
// content and, when it is the content of a `scroll_view`, scrolls the caret
// into view.
//
// Input method composition is shown in place at the caret and underlined; the
// committed text arrives as ordinary text input. Edits made by the user (and
// by assistive technology) call the change handler; programmatic changes do
// not. There is no undo here: application editors keep their own history.
class text_field : public widget {
public:
  explicit text_field(std::string text = {}, bool multiline = false);

  [[nodiscard]] auto text() const -> std::string const & { return text_; }
  // Replaces the content without calling the change handler; the caret goes
  // to the end.
  auto set_text(std::string text) -> void;

  [[nodiscard]] auto multiline() const -> bool { return multiline_; }
  auto set_multiline(bool multiline) -> void;
  [[nodiscard]] auto placeholder() const -> std::string const & { return placeholder_.text(); }
  auto set_placeholder(std::string placeholder) -> void;
  [[nodiscard]] auto read_only() const -> bool { return read_only_; }
  auto set_read_only(bool read_only) -> void;
  // Text and spacing of a source or reading editor use a different style.
  [[nodiscard]] auto role() const -> text_role { return role_; }
  auto set_role(text_role role) -> void;
  // Without a frame the field draws no background or border, for use as the
  // content of an editor view.
  [[nodiscard]] auto framed() const -> bool { return framed_; }
  auto set_framed(bool framed) -> void;
  // The width wanted when the parent does not constrain it.
  [[nodiscard]] auto preferred_width() const -> length { return preferred_width_; }
  auto set_preferred_width(length width) -> void;
  // What assistive technology calls the field, such as "Find".
  [[nodiscard]] auto accessible_name() const -> std::string const & { return name_; }
  auto set_accessible_name(std::string name) -> void { name_ = std::move(name); }

  [[nodiscard]] auto selection() const -> accessibility::text_selection {
    return {anchor_, caret_};
  }
  [[nodiscard]] auto selection_begin() const -> std::size_t;
  [[nodiscard]] auto selection_end() const -> std::size_t;
  [[nodiscard]] auto has_selection() const -> bool { return anchor_ != caret_; }
  [[nodiscard]] auto selected_text() const -> std::string_view;
  // Offsets are clamped and moved back to a grapheme boundary.
  auto set_selection(std::size_t anchor, std::size_t focus) -> void;
  auto select_all() -> void;

  // Replaces the selection with `text` as typing would. False when the field
  // is read-only.
  auto insert_text(std::string_view text) -> bool;

  // In-progress input method text, shown at the caret.
  [[nodiscard]] auto preedit() const -> std::string const & { return preedit_; }

  // The caret in this widget's coordinates: a zero-width rectangle as tall as
  // the line. Platforms use it to place the input method window.
  [[nodiscard]] auto caret_rect() const -> rect;
  // The caret offset nearest `position` (this widget's coordinates).
  [[nodiscard]] auto offset_at(point position) const -> std::size_t;

  auto set_clipboard(text_clipboard clipboard) -> void { clipboard_ = std::move(clipboard); }
  // Called with the new text after every user edit.
  auto set_on_change(std::function<void(std::string const &)> handler) -> void {
    on_change_ = std::move(handler);
  }
  // Called when Enter is pressed in a single-line field.
  auto set_on_submit(std::function<void(std::string const &)> handler) -> void {
    on_submit_ = std::move(handler);
  }

  [[nodiscard]] auto accessibility_info() const -> accessibility::info override;

protected:
  auto on_event(event_context &context, event const &e) -> bool override;
  auto on_paint(render_context &context) const -> void override;
  [[nodiscard]] auto on_measure(constraints const &limits) const -> basalt::size override;
  auto on_interaction_changed(interaction_state const &previous) -> void override;
  auto on_accessibility_action(accessibility::action_request const &request) -> bool override;

private:
  struct cache {
    text::text_shaper const *shaper = nullptr;
    text_style style;
    std::string text;
    float width = 0;
    bool wrap = false;
    std::shared_ptr<text::paragraph const> laid_out;
    bool valid = false;
  };

  enum class unit_of_move : std::uint8_t { grapheme, word, line, document };

  [[nodiscard]] auto padding() const -> insets;
  [[nodiscard]] auto content_width() const -> float;
  // The text as drawn: the content with the preedit inserted at the caret.
  [[nodiscard]] auto display_text() const -> std::string;
  [[nodiscard]] auto display_caret() const -> std::size_t;
  [[nodiscard]] auto from_display(std::size_t offset) const -> std::size_t;
  [[nodiscard]] auto paragraph_for(float width) const -> text::paragraph const *;
  [[nodiscard]] auto line_height() const -> float;
  [[nodiscard]] auto content_origin() const -> point;
  [[nodiscard]] auto boundaries() const -> std::vector<std::size_t> const &;
  [[nodiscard]] auto snap(std::size_t offset) const -> std::size_t;
  [[nodiscard]] auto previous_boundary(std::size_t offset, unit_of_move unit) const -> std::size_t;
  [[nodiscard]] auto next_boundary(std::size_t offset, unit_of_move unit) const -> std::size_t;
  [[nodiscard]] auto line_bounds(std::size_t offset) const -> std::pair<std::size_t, std::size_t>;

  auto move_caret(std::size_t to, bool extend) -> void;
  auto move_vertically(int direction, bool extend) -> bool;
  auto selection_changed() -> void;
  auto reveal_caret() -> void;
  auto replace_range(std::size_t begin, std::size_t end, std::string_view replacement) -> void;
  auto notify_change() -> void;
  auto set_preedit(std::string preedit) -> void;
  auto handle_key(key_event const &k) -> bool;
  auto handle_pointer(pointer_event const &p) -> bool;

  std::string text_;
  text_line placeholder_;
  std::string name_;
  std::string preedit_;
  text_role role_ = text_role::body;
  length preferred_width_ = fixed(200);
  bool multiline_ = false;
  bool read_only_ = false;
  bool framed_ = true;
  bool dragging_ = false;
  std::size_t anchor_ = 0;
  std::size_t caret_ = 0;
  text::caret_affinity affinity_ = text::caret_affinity::downstream;
  // The x that vertical movement tries to keep, in paragraph coordinates.
  std::optional<float> goal_x_;
  // Single-line horizontal scroll.
  float scroll_x_ = 0;
  text_clipboard clipboard_;
  std::function<void(std::string const &)> on_change_;
  std::function<void(std::string const &)> on_submit_;
  mutable cache cache_;
  mutable std::vector<std::size_t> boundaries_;
  mutable bool boundaries_valid_ = false;
};

} // namespace basalt
