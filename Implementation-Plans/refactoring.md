# Plan: reduce duplication, improve robustness and performance in Basalt

## Summary

This plan fixes one bug in paragraph layout, removes repeated code from the widgets and the text
layer, and addresses the main avoidable costs in measuring, painting and shaping. The work is split
into six phases that can each be merged on their own. Phase 1 is the bug fix. Phase 2 adds shared
widget helpers and moves the existing widgets onto them. Phases 3 and 4 remove duplication in the
widgets and in the core and text code. Phase 5 changes caching. Phase 6 covers items that need
profiling before any change. The findings come from reading the source. Nothing has been profiled
yet, so the performance estimates are expectations to confirm.

## Findings

### Bug

`rebased` in `src/basalt/text/paragraph.cpp` rebuilds each style span with only `begin`, `end`,
`bold` and `italic`. The `scale`, `rise` and `capitals` fields reset to their defaults, so a
`paragraph` or `text_layout` renders superscripts, subscripts and small capitals as plain text. Only
`text_shaper` has tests for those fields.

### Duplicated code

- **Theme lookup.** Ten files define their own `theme_of`, and nineteen places build a local
  `basalt::theme const fallback;`. Each local fallback copies the whole token set, which has about
  970 fields, on every measure and paint, even when the widget is attached.
- **Press activation.** About ten widgets repeat the same `on_event` body: a primary press is
  consumed, a release while pressed activates, and an unmodified, non-repeating Enter or Space
  activates. The copies are in button, menu, accordion, popover, diagnostics list, navigation bar,
  choice control and tabs. Tabs does not check `repeat`, so a held key reselects the tab.
- **Safe handler calls.** Eleven places copy a callback to a local before calling it, because the
  callback may destroy the widget.
- **State colors.** The `pressed ? active : hovered ? hover : normal` ladder appears in button,
  checkbox, radio button, toggle and chip.
- **Field text.** `select` and `choice_control` both hold a label, helper text and error flag with
  the same setters and the same measurement code.
- **Invalidation.** `invalidate_layout(); invalidate_paint();` appears together 48 times.
- **Ancestor lookup.** There are 21 hand-written parent walks and 20 `dynamic_cast` parent lookups.
  `text_field` and `diagnostics_list` contain the same "find the enclosing scroll view and scroll
  into view" code.
- **Core.** The ancestor id chain is built three times (`event_router.cpp`, `focus.cpp`). The
  "restore focus if still valid" block appears twice in `focus.cpp`. The file-drop leave event is
  sent from two places in `event_router.cpp`. `checkbox_group` and `radio_group` have identical
  `accessibility_info`.
- **Text.** Glyph clusters are grouped in two places in `paragraph.cpp`. Offsetting a glyph run's
  clusters is done in `paragraph.cpp` and `text_shaper.cpp`. The Unicode mark test is in both of
  those files. `break_marker_em` is defined in `paragraph.cpp` and `text_layout.cpp`. Finding a
  caret stop for an offset is open-coded five times in `paragraph.cpp`. `append_utf8` lives in
  `text_shaper.cpp` and belongs next to `decode_utf8`.

### Readability and robustness

- `paragraph::break_into_lines` is about 145 lines long.
- `event_router::dispatch` uses an `if constexpr` chain inside `std::visit`.
- `shape_options::capital` is a raw function pointer. `spec/CONVENTIONS.md` prefers `std::function`.
- `text_shaper::shape` is `const` but `shape_segment` sets the scale on the face's shared HarfBuzz
  font. Two threads that shape through one `font_manager` would race.

### Performance

- `select` builds a temporary `text_line` in `parts()` and `on_paint`, so the value text is shaped
  again on every pass. `chosen_indices()` allocates twice per pass.
- `text_line::shaped` compares the full string on every call and keeps a second copy of it.
- `text_field::paragraph_for` builds `display_text()` and compares it on every call, and it is
  called several times per frame. A multiline edit lays out the whole text again, although
  `text_layout` supports block-level updates.
- `text_shaper::shape` creates a HarfBuzz buffer, a font request with a `std::string` and a bidi
  resolution on every call. Line breaking calls it several times per line.
- `accessibility::build` calls `bounds_in_root`, `enabled_in_tree` and `is_blocked` per node, and
  each walks to the root, so the cost is nodes times depth.

## Approach

Every phase keeps public headers source-compatible unless stated. Each phase ends with `just test`
and `just test sanitize` passing, and with all four compilers in CI. New helpers go in public
headers only when widgets outside the library need them. Otherwise they go in `src/basalt/`.

Per `AGENTS.md`, each new test is made to fail once on purpose before it is trusted.

## Phase 1: fix span handling in paragraph layout

1. Change `rebased` to copy each span and adjust only `begin` and `end`.
2. Add a `paragraph` test with a span that sets `scale`, one that sets `rise` and one that sets
   `capitals`. Each test checks the resulting glyph size, baseline or glyph ids. Run each test
   against the old `rebased` first to confirm it fails.
3. Add the same check to a `text_layout` test, since `text_layout` builds paragraphs through the same
   path.

## Phase 2: shared widget helpers

1. Add `widget::theme() const -> theme const &`. It returns the tree's theme, or one static default
   theme while detached. Replace every `theme_of` and every local `fallback` with it. Where a widget
   currently reads `tree()->theme()` after a null check, leave the null check and use the accessor
   for the value.
2. Add `widget::find_ancestor<T>()` and use it for all `dynamic_cast` parent lookups and for the
   scroll view search. Add `scroll_into_view_of(widget&, rect)` as a private helper for the two
   widgets that scroll themselves into view.
3. Add `widget::invalidate_content()` that invalidates layout and paint, and use it where both calls
   are made together.
4. Add `notify(handler, args...)` in `src/basalt/`. It copies the callable, then calls it, and holds
   the comment about widget destruction. Replace the eleven copy-then-call sites.
5. Add `by_state(control_state, normal, hovered, pressed)` next to `control_state_of` in
   `interaction.h`. Replace the color ladders.

## Phase 3: shared activation and field text

1. Add a `press_activation` helper in `src/basalt/widgets/`. It takes the widget's interaction state
   and an activation callback, and implements the pointer and key rules once. It has an option for
   extra accepted keys (navigation bar uses Down, choice control uses Enter conditionally).
2. Move button, menu item, accordion header, popover, diagnostic row, navigation item, choice
   control and tab onto it. Tabs gains the `repeat` check as part of this change.
3. Add a `field_text` component that owns the label, helper and placeholder `text_line`s, the error
   flag and the size role, with setters that report whether anything changed. Use it in
   `select` and `choice_control`.
4. Give `checkbox_group` and `radio_group` a shared base or helper for their group
   `accessibility_info`.

## Phase 4: core and text cleanup

1. Add one `ancestor_chain(widget*)` helper and use it in `event_router.cpp` and `focus.cpp`.
2. Extract `restore_focus_to(widget_id)` in `focus.cpp` and `send_drop_leave(widget_id, ...)` in
   `event_router.cpp`.
3. Move `append_utf8` into `utf8.h`. Add a shared `is_mark(char32_t)` and `break_marker_em` constant
   in `src/basalt/text/`.
4. Add a `clusters(run)` range based on `std::views::chunk_by` and use it in both `paragraph.cpp`
   loops. Add `offset_clusters(glyph_run&, size_t)` and use it in `paragraph.cpp` and
   `text_shaper.cpp`.
5. Split `paragraph::break_into_lines` into four functions: choose the line end, compute line
   metrics, compute the alignment offset, and apply inline gaps to the runs. Output must be
   unchanged, so the existing paragraph tests are the check.
6. Replace the `if constexpr` chain in `event_router::dispatch` with an overloaded visitor.
7. Change `shape_options::capital` to `std::function<char32_t(char32_t)>`. This changes a public
   type, so note it in the release notes.

## Phase 5: caching

1. `select`: keep the value `text_line` as a member and update it when the choice or placeholder
   changes. Compute the chosen count once per pass.
2. `text_line`: replace the string comparison with a dirty flag set by `set_text` and `set_role`,
   and compare only theme, scale and shaper identity on each call. Drop the stored copy of the
   text.
3. `text_field`: invalidate the cached paragraph from the places that change text, preedit, role,
   width or multiline, so `paragraph_for` returns without building `display_text()`.
4. Evaluate moving multiline `text_field` onto `text_layout` so edits re-lay out only the touched
   block. This is a larger change and gets its own decision after steps 1 to 3 are measured.

## Phase 6: items that need measurements first

1. **Shaper allocation.** Build a benchmark that lays out a long paragraph with wrapping. If
   `shape` dominates, reuse the HarfBuzz buffer and the primary-face lookup across calls, and run
   bidi resolution once per paragraph in `shape_spans`.
2. **Accessibility build.** Pass origin, enabled state and blocked state down the recursion in
   `accessibility::build` so each node costs constant time. Measure on a tree of a few thousand
   widgets before and after.
3. **Shaper thread safety.** Decide whether shaping is documented as single-threaded or whether each
   call gets its own scaled font through `hb_font_create_sub_font`. The documentation option costs
   nothing. The second option has a per-call cost that the benchmark above should show.

### Phase 6 outcome

Benchmarks `text_shaper` and `accessibility` were added to `benchmarks/widgets_benchmark.cpp`.
Sampling showed bidi resolution at about a quarter of shaping time and face lookup as negligible, so
the work went to bidi rather than to the face lookup.

1. **Shaper allocation.** The HarfBuzz buffer is now reused per thread, and `resolve_bidi` returns
   early for ASCII text in a left-to-right paragraph. A 400 word wrapped paragraph went from about
   2.23 ms to 1.79 ms, and shaping one line from about 4.9 us to 3.6 us. The primary-face cache and
   the single bidi pass in `shape_spans` were not done, because the profile gave no reason for them.
2. **Accessibility build.** Passing origin, enabled and modal state down the recursion was
   implemented and measured on 3000 nodes at depth 12. It saved about 2% with overlapping intervals,
   so it was reverted. The cost is elsewhere in the node and vector construction.
3. **Thread safety.** Shaping is documented as single-threaded per `font_manager` in `text_shaper.h`.

## Risks

- Phase 2 touches nearly every widget source file. Merging it as one change keeps the helpers and
  their callers consistent, but the diff is large. Splitting it by helper is possible, since each
  helper is independent.
- The activation helper changes tab behavior for held keys. This is intended, and the tab tests
  need a case for it.
- Moving `text_line` to a dirty flag relies on every mutation going through `set_text` and
  `set_role`. Both are the only mutators today, and the member is private.
- Changing `shape_options::capital` breaks source compatibility for consumers that pass a function
  pointer to a context that cannot convert. A plain function or captureless lambda still converts.
- Performance items in phases 5 and 6 are expected improvements, not measured ones. Each change
  should be kept only if a benchmark shows a gain.

## Verification

- `just test` and `just test sanitize` pass after each phase.
- `just consumer` passes after phases that touch public headers (2, 3 and 4).
- Rendered output is unchanged for existing tests, which use paint-command snapshots for most
  widgets.
- Phase 1 tests fail against the old `rebased` and pass against the fix.
