# Plan: Text Field Focus Loss and Cancel

This change adds two handlers to `text_field`. `set_on_focus_lost` is called when the field loses keyboard focus after a user edit. `set_on_cancel` is called when the user presses Escape. Forms that commit a value without a confirm button need both. The first user is med's inspector panel, where a field commits when the writer leaves it and Escape restores the shown value. The change is small and stays inside `text_field`, because the focus manager already clears the field's `focused` flag at every focus change. The documentation and tests change with it.

## Current State

`text_field` has `set_on_change`, called after every user edit, and `set_on_submit`, called on Enter in a single-line field. It has no handler for focus loss and does not handle Escape, so an Escape key event goes on to the parent.

The focus manager already gives the field what it needs. `focus_manager::refresh_flags` clears `interaction_state::focused` on the widget that loses focus, and `text_field::on_interaction_changed` already sees the change: it stops a drag and clears the preedit. Focus moves away from a field in these cases:

- Another widget takes focus by Tab, by pointer or by a call.
- `clear_focus` is called.
- A modal is pushed.
- The field or an ancestor is hidden, which moves focus to the nearest ancestor that accepts it.

Two cases do not clear the flag:

- When the window becomes inactive, `set_window_active(false)` leaves the field focused, and only the focus ring is hidden.
- When the field leaves the tree, `refresh_flags` cannot find it by id, so its flag is not cleared and it gets no call.

`refresh_flags` resolves widgets by id because hooks run from it may change the tree.

## Behavior

The field tracks whether it has a user edit that is not confirmed yet. The flag is set by every user edit, which are the edits that call the change handler. Enter (after the submit handler), `set_text`, and the focus-loss call itself clear the flag.

- **Focus loss.** When `on_interaction_changed` sees `focused` go from true to false and the flag is set, the field clears the flag, then calls the focus-loss handler with its text. Focus loss with no unconfirmed edit makes no call. Enter followed by Tab therefore commits once.
- **Window deactivation.** No call. The field keeps focus while the user switches to another application, as native text fields do.
- **Leaving the tree.** No call. The field is going away, and calling out from inside a detach is not safe. Owners that rebuild a form must commit first. This is stated in the header.
- **Escape.** When a cancel handler is set, Escape clears the flag, calls the handler, and consumes the event. When no handler is set, Escape is not handled, as today, so dialogs and popovers still close on Escape. The handler usually calls `set_text` with the stored value. Escape during input method composition stays with the input method and makes no call.
- **Read-only fields.** A read-only field has no user edits, so neither handler is called.

Each handler is copied to a local before it is called, and the field does not touch its members afterwards. This follows the submit path, and it lets a handler hide, rebuild or remove the field. `set_on_change` and `set_on_submit` do not change.

## API

```cpp
// Called with the text when the field loses focus after a user edit that Enter has not confirmed.
// It is not called when the window becomes inactive or when the field leaves the tree.
auto set_on_focus_lost(std::function<void(std::string const &)> handler) -> void;
// Called when Escape is pressed. When it is set, the field consumes Escape.
auto set_on_cancel(std::function<void()> handler) -> void;
// True after a user edit that Enter, Escape, `set_text` or a focus-loss call has not yet ended.
[[nodiscard]] auto has_unconfirmed_edit() const -> bool;
```

`has_unconfirmed_edit` lets an owner decide whether to commit before a rebuild, which covers the case where a field leaves the tree.

## Accessibility

A `set_value` action from assistive technology counts as a user edit, because it already calls the change handler. Focus that moves by an accessibility `focus` action goes through the focus manager like any other move, so the focus-loss call follows. The accessibility info does not change.

## Documentation

`docs/widgets/text_field.md` gets a section on committing values. It says when each handler is called, when it is not, and that an owner must commit before it removes a field. The section is written in ASD-STE100 style, as `CLAUDE.md` requires. The class comment in `text_field.h` gets the same rules in short form.

## Tests

The tests go in `tests/basalt/text_field_test.cpp` and use the existing UI fixture with two fields in a tree. They cover:

- Typing then Tab calls focus-lost once with the text.
- Tab with no edit makes no call.
- Enter then Tab calls submit and no focus-lost.
- A pointer press on the other field after an edit calls focus-lost.
- Hiding the field's parent after an edit calls focus-lost.
- Pushing a modal after an edit calls focus-lost.
- Window deactivation after an edit makes no call, and a Tab after reactivation does.
- Removing the field from the tree after an edit makes no call and does not crash.
- `set_text` clears the unconfirmed edit.
- An accessibility `set_value` sets it.
- Escape with a handler calls it and is consumed.
- Escape with no handler is not consumed.
- Escape during composition makes no call.
- A focus-lost handler that removes the field from the tree runs cleanly under the sanitizer build.

Each new test is run once against a deliberate break before it is trusted, as `CLAUDE.md` requires. The breaks planned are:

- Not clearing the flag on Enter, which makes Enter then Tab call twice.
- Calling on window deactivation.
- Consuming Escape with no handler set.
- Touching a member after the handler, which the sanitizer build reports.

## Risks

- A widget that leaves the tree is never told it lost focus. The rule is documented, and `has_unconfirmed_edit` gives owners a way to commit first. A detach notification for widgets would be a larger change to the focus manager, and it is out of scope here.
- Consuming Escape only when a handler is set keeps current behavior for existing users. A form inside a dialog that sets a cancel handler stops Escape from closing the dialog while the field has focus. Such a dialog closes from a button or a second Escape after focus leaves the field. The documentation says so.
- The change adds API, so it needs a minor version bump. med takes it after the release.
