## Tabs

A tab bar is a row of tabs. The user picks one tab to choose a view. The bar reports the selection.
Your program shows the matching content. The bar does not hold the content.

![A tab bar with the second tab selected](../images/widgets/tabs.png)

### Create and configure

Create a `tab_bar` with a name. Call `add_tab(label)` for each tab. It returns the `tab`.

- `select(index)` selects a tab and clears the others. It returns `false` when the index is out of
  range. Unlike most setters, it calls the select handler when the selection changed.
- `selected_index()` returns the selected index, or an empty value.
- `tab_at(index)` and `tab_count()` read the tabs.
- `tab::set_closable(true)` shows a close button at the end of a tab.
- `scroll_tab_into_view(index)` scrolls a long row to show a tab.
- `set_style(tab_bar_style::segmented)` changes the look, as described below.

Tabs shrink toward a minimum width as the row fills. When they no longer fit, the row scrolls
sideways. The wheel or the trackpad scrolls it. A vertical wheel turns sideways.

### Overflow controls

When the tabs do not fit, the bar shows a fade at each edge that has more tabs past it and a scroll
button at each end. A scroll button moves the row just far enough to show the next partly hidden
tab, and repeats while held. If an `overlay_host` is above the bar, a third button opens a menu of
every tab. Choosing an item selects the tab and scrolls it into view. Without a host, the bar shows
no menu button.

The buttons paint the bar's background so that no tab shows beneath them. Call
`set_background(&tokens::color_set::background_secondary)` when the bar sits on a different color.
The buttons are not Tab stops. Left and Right scroll the row as they select, and Down on a tab opens
the menu. `scroll_toward_start()`, `scroll_toward_end()` and `open_tab_menu()` do the same from code.

### Segmented style

`set_style(tab_bar_style::segmented)` divides the width equally among the tabs inside a rounded
group. The bar never scrolls and shows no overflow controls. Labels that do not fit are shortened
in the middle. Use it for a few options, such as a mode switch. Selection, focus and keyboard
behavior are the same as in the default `line` style.

### Events

`set_on_select` calls its handler with the index of the newly selected tab. `set_on_close` calls its
handler with the index of a tab whose close button the user pressed. The bar does not remove the tab.
Remove or rebuild the tabs in the handler.

The tabs form one focus group. Tab enters at the selected tab. Left, Right, Home and End move among
the tabs and select as they go.

### Example

The program `examples/widgets/tabs.cpp` makes a bar with three tabs, one of them closable. It clicks the second tab and prints the selection.

```{.cpp include="examples/widgets/tabs.cpp"}
```
