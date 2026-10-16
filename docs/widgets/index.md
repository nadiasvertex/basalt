# Widgets

This chapter describes each widget in Basalt. Every page states what the widget is for, how to
create and control it in code, and which events it reports. Every page also names a short example
program in `examples/widgets/` and shows a screenshot of its output. The pages follow the same
order and use the same headings, so you can compare widgets quickly.

## Common rules

All widgets follow these rules. The pages repeat a rule only when the widget changes it.

- The tree owns every widget. Use `emplace_root` on the tree and `emplace_child` on a parent to
  create a widget. Both return a reference to the new widget. The reference stays valid until you
  remove the widget.
- Most setters that you call from code do not call the change handler of the widget. Handlers run
  when the user, or assistive technology, changes the value. `tab_bar::select` is an exception, and
  its page says so.
- A disabled widget ignores input. Call `set_enabled(false)` to disable a widget.
- Each widget describes itself to assistive technology in `accessibility_info()`. Give controls
  that have no visible text an accessible name.
- A widget that is not a child of an `overlay_host` cannot show popups. Menus, selects, popovers
  and dialogs need an `overlay_host` above them.

## Run the examples

Each example is a small program without a window. It builds one widget, sends simulated input
to it, and prints what the handlers received. The programs need CMake, Ninja, and the sources of
Skia. Skia only draws the screenshot.

```sh
just widget-examples
.build/widgets/button
```

To write a screenshot, give the program a file name. The file uses the PAM format. The
`just screenshots` recipe runs every program and converts the files to the PNG images in
`docs/images/widgets/`. The conversion needs Python with Pillow.

```sh
.build/widgets/button button.pam
```

Examples use the `example_host` class from `examples/widgets/common/`. It makes a tree with
fonts and a router, and it sends clicks, keys and text. A real program uses its own window
system for this work, as `examples/sdl_skia` shows.

## Widget list

| Widget | Purpose | Classes |
| ------ | ------- | ------- |
| [Accordion](accordion.md) | Sections that expand and collapse | `accordion`, `accordion_item` |
| [Avatar](avatar.md) | A person or entity as a photo or initials | `avatar` |
| [Box](box.md) | A row or column of children | `box` |
| [Button](button.md) | An action | `button` |
| [Checkbox](checkbox.md) | An independent choice, alone or in a group | `checkbox`, `checkbox_group` |
| [Chip](chip.md) | A compact tag, filter or removable value | `chip`, `chip_group` |
| [Dialog](dialog.md) | A message with buttons | `dialog` |
| [Diagnostics list](diagnostics_list.md) | Errors, warnings and notes | `diagnostics_list` |
| [Image view](image_view.md) | One picture | `image_view` |
| [Label](label.md) | One line of text | `label` |
| [Menu](menu.md) | A list of commands | `menu`, `menu_item` |
| [Menu bar](menu_bar.md) | Menus along the top of a window | `menu_bar` |
| [Navigation bar](navigation_bar.md) | The top bar of an application | `navigation_bar`, `navigation_item` |
| [Overlay host](overlay_host.md) | The root that shows popups and dialogs | `overlay_host` |
| [Palette](palette.md) | A searchable list of commands | `palette` |
| [Popover](popover.md) | A panel beside a trigger | `popover` |
| [Progress bar](progress_bar.md) | How far an operation has come | `progress_bar` |
| [Radio button](radio_button.md) | One choice from a set | `radio_button`, `radio_group` |
| [Scroll bar](scroll_bar.md) | A track with a thumb | `scroll_bar` |
| [Scroll view](scroll_view.md) | A window onto larger content | `scroll_view` |
| [Select](select.md) | A value chosen from a list | `select` |
| [Separator](separator.md) | A line between groups | `separator` |
| [Split view](split_view.md) | Two panes and a divider | `split_view`, `splitter` |
| [Stack](stack.md) | Layers of children | `stack` |
| [Status bar](status_bar.md) | Facts along the bottom of a window | `status_bar` |
| [Tabs](tabs.md) | A row of tabs | `tab_bar`, `tab` |
| [Text field](text_field.md) | Editable text | `text_field` |
| [Toggle](toggle.md) | An on/off switch | `toggle` |
| [Tooltip](tooltip.md) | A short hint | `tooltip`, `tooltip_anchor` |
| [Uploader](uploader.md) | Files from a dialog or a drop | `uploader` |

## Shared parts

Several widgets use the same building blocks. You meet them in the widget APIs.

| Type | Purpose |
| ---- | ------- |
| `control_size` | The Cubit sizes `small`, `medium` and `large`. Controls take it as a constructor argument. |
| `text_role` | The theme text style of a label or control, such as `body` or `heading_large`. |
| `text_tone` | The semantic color of a label, such as `danger` or `success`. |
| `text_line` | One line of text that remembers its shaping. Custom widgets use it to draw text. |
| `field_text` | The label, helper text, placeholder and error flag of a form control. |
| `choice_control` | The base of `checkbox`, `radio_button` and `toggle`. It has a label, helper text, error state and `activate()`. |
| `hover_delay` | A countdown for tooltips and hover popovers. |
