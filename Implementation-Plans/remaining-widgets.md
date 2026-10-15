# Plan: implement the remaining Cubit widgets in Basalt

## Summary

This plan adds the eleven Cubit components that Basalt lacks: Toggle, Checkbox, Radio Button, Chip,
Select, Accordion, Popover, Avatar, Navigation Bar and Uploader, plus a minimal Tooltip that Chip and
Avatar need. The work is split into five phases. Phase 1 adds shared pieces that several widgets
depend on. Phases 2 to 5 add the widgets in dependency order. The five spec pages that are still
empty (List, Search, Side Panel, Toolbar and the full Tooltip page) are left out until the spec is
written. Each widget follows the existing pattern: one public header, one source file, one test file,
a CMake entry, an include in `basalt/basalt.h` and a line in the README widget list.

## Findings that shape the plan

- Widgets derive from `widget` (`include/basalt/widget.h`) and override `on_event`, `on_paint`,
  `on_measure`, `accessibility_info` and `on_accessibility_action`. `button` is the model for a
  simple control and `menu` is the model for a focus group.
- The theme already holds the tokens these widgets need: checkbox box, toggle track and handle,
  avatar, accordion header, popover width, chip text inset, drop zone inset and tooltip inset.
- `render_context` can draw rectangles, rounded rectangles, lines, glyph runs and images. There is no
  icon API. Chevrons, check marks, minus signs and close marks can be drawn with `line`.
- `accessibility::role` has `check_box` and `radio_button` but no switch, combo box, tooltip or
  navigation role. `check_state` already has `mixed`.
- `focus_scope::group` exists and is used by tabs and menus, so radio and chip groups can reuse it.
- `overlay_host::show_popup` places a popup below or above an anchor and dismisses it on an outside
  press or Escape. It has no non-modal mode, which hover popovers and tooltips need.
- `event.h` has no file-drop event and the library has no way to ask the host for a file dialog. The
  uploader needs both.
- `text_field` has no label, helper text or error state, which Select and the Checkbox and Radio
  groups also need.

## Phase 1: shared foundations

These pieces come first because more than one widget uses each of them.

1. **Control size.** Add a `control_size` enum (small, medium, large) that maps to the matching body
   text role and token set. Spec pages say size follows body text size.
2. **Glyph drawing helpers.** Add a private header in `src/basalt/widgets/` that draws chevron, check,
   indeterminate and close marks with `render_context::line`, so widgets share one implementation.
3. **Field frame.** Add a `field_frame` helper or container that lays out a label, a control and
   helper or error text with a status indicator. Checkbox, radio, toggle, select and text field all
   use this anatomy. Extending `text_field` to use it is a follow-up, not part of this plan.
4. **Roles.** Add `switch_control`, `combo_box`, `list_box`, `tooltip` and `navigation` to
   `accessibility::role`, update `to_string`, `is_interactive` and `requires_name`, and update the
   accessibility test.
5. **Non-modal popups.** Extend `overlay_host` with a hover popup that does not block the pointer or
   trap focus, and with an optional tip that points at the anchor.
6. **Hover delay.** Add a small delay helper that uses `request_animation_frame` and the elapsed time
   from `on_animation_frame`, for tooltips and hover popovers.

## Phase 2: selection controls

Each control is a focusable widget with a label, uses `control_state_of` for appearance and reports
state through `accessibility::info`.

- **toggle** (`toggle.h`): on/off with track and handle, optional status text, immediate effect.
  Space and Enter toggle it. Role `switch_control`.
- **checkbox** (`checkbox.h`): unchecked, checked and mixed, with an error state. A `checkbox_group`
  owns a group heading and helper text, and supports a parent checkbox whose state follows its
  children.
- **radio_button** and **radio_group** (`radio_button.h`): the group is one Tab stop with arrow-key
  movement through `focus_scope::group`. Selection is exclusive and the group enforces it.
- **chip** and **chip_group** (`chip.h`): selectable chips with single or multiple selection, and
  removable chips with a remove button, an error state and a loading state. The group wraps to
  multiple lines and has a Remove All button. Long text is truncated and shown in a tooltip.
- **select** (`select.h`): a field showing the value or placeholder, opening a list in a popup from
  `overlay_host::show_popup`. Single selection closes on choice. Multiple selection shows a count
  with a clear button, keeps the list open on choice and sorts selected items first on reopen. The
  popup list reuses `menu` item behavior where it can, and scrolls through `scroll_view`.

## Phase 3: disclosure and overlays

- **tooltip** (`tooltip.h`): a minimal hover and focus tooltip built on the non-modal popup and the
  hover delay from Phase 1. The spec page is empty, so this follows the popover spec's mention of
  tooltips and nothing further.
- **popover** (`popover.h`): a trigger plus content, opened on hover for short text or on click for
  interactive content. It supports an optional tip, a single open popover at a time, and uses the
  token width limits.
- **accordion** (`accordion.h`): items with a header, chevron, heading, optional item actions and
  content. Supports left and right chevron layouts, the three appearances, expand and collapse all,
  and sizes. Headers behave as buttons and report `expanded` in the accessibility info.

## Phase 4: identity and navigation

- **avatar** (`avatar.h`): image or initials graphic, with trailing, leading or graphic-only layouts,
  name and supporting text. Initials are derived from the name. The graphic-only layout shows the
  name in a tooltip. Uses `image_view` for photos.
- **navigation_bar** (`navigation_bar.h`): the navigation sections layout and the toolbar layout,
  with product logo and menu, title, sections that open submenus, utility links, an avatar and
  leading and center actions. Items have default, hover and selected states. It composes `button`,
  `menu`, `avatar` and the popover.

## Phase 5: uploader

- Add a file-drop pointer event and a host callback for opening a file dialog, both in the core
  layer, with tests in `event_router_test.cpp`. Basalt never touches the operating system, so the
  library defines the hook and the application supplies it, as it does for `text_clipboard`.
- **uploader** (`uploader.h`): the no drop zone, compact and spacious layouts, drag and error states,
  and the three behaviors (immediate, deferred single file, deferred multiple files). It uses
  `button` for Select Files and `chip_group` for the file list.

## Deferred

List, Search, Side Panel, Toolbar and the full Tooltip page have placeholder text in `_pending/`.
They should wait until the spec is filled in. Toolbar and List are the likeliest next items, since
Navigation Bar and Search build on them.

## Conventions and tests

- Follow `spec/CONVENTIONS.md`: snake_case, `auto f() -> T`, `#pragma once`, value semantics, no raw
  owning pointers, warnings as errors on GCC, Clang, Apple Clang and MSVC.
- Every widget gets a Catch2 test file that uses `ui_fixture.h` and `bundled_fonts()`. Tests cover
  states, keyboard operation, pointer operation, layout at each size and the accessibility info.
- Before trusting each test, make it fail on purpose by breaking the code or the expected value,
  confirm the failure message and undo the change, as `AGENTS.md` requires.
- New headers must pass the `include_boundaries` test: quoted includes start with `basalt/`, no
  public header includes HarfBuzz, and `basalt/basalt.h` includes every public header.
- After approval, copy this plan to `Implementation-Plans/` in the repository.

## Verification

- `just test` after each widget, and `just test sanitize` at the end of each phase.
- `just consumer` at the end of Phase 5 to confirm the installed headers work for a consumer.
- `just format` before each commit.
- A manual check of the accessibility snapshot for each widget against its spec page states.

## Assumptions

- Icons are drawn from lines and shapes, not loaded from an icon font or image set.
- Phase 5 changes the core event layer. If the file-drop work should wait, Phase 5 can be dropped
  without affecting the other phases, except that Chip works without it.
- The deferred spec pages are out of scope until they are written.
