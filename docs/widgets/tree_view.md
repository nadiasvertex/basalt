## Tree view

A tree view shows nested rows that the user can expand and collapse. Use it for an outline, a
folder tree or any other hierarchy. The tree makes widgets only for the rows that you can see.
Thus a tree with many thousands of rows costs about the same as a tree with a few rows.

![A tree of folders and files with one row selected](../images/widgets/tree_view.png)

### Create and configure

Create a `tree_view` with an accessible name. Call `set_rows` with a `tree_rows` value. It holds
two functions: `count` gives the number of rows, and `row` gives the `tree_row` at an index. The
rows are the visible rows in display order. The descendants of a collapsed row are not rows.

A `tree_row` has these fields:

- `text`: the label of the row.
- `detail`: secondary text. The tree shows it at the end of the row in a subdued color.
- `depth`: 0 for a top-level row.
- `expandable` and `expanded`: whether the row has a chevron, and which way it points.
- `position_in_set` and `set_size`: the position of the row among its siblings, and their count.
  Assistive technology uses them. Set them when you know them, because the tree cannot count
  rows that it did not make.

`tree_rows` has a third, optional function, `depth`. It gives the depth of a row without its
text. The tree uses it when it walks over many rows, for example to find the parent of a row.

The tree keeps no copy of your data. When the data changes, call `rows_changed()`.

- `select(index)` selects a row, and `select(std::nullopt)` clears the selection. The tree does
  not scroll, and it does not take focus. If focus is in the tree, focus moves to the new row.
- `selected()` gives the selected row.
- `scroll_to_row(index)` scrolls as little as possible to show the row.
- `row_area(index)` gives the area of a row in the coordinates of the tree.

The tree has no natural width. Give it a width with its layout, for example with
`set_cross_alignment(alignment::stretch)` on the parent box. Give it a limited height, because
it scrolls only when its rows are taller than the tree.

### Events

Your program owns the expanded state. When the user asks to expand or collapse a row, the tree
calls the handler from `set_on_expand` with the row index and the new state. The handler must
change the data and call `rows_changed()`. The selection moves with the change: a row below the
subtree keeps its selection, and a hidden row gives its selection to the collapsed row.

The rows are one Tab stop. These keys move the selection and call the handler from
`set_on_select`:

- Up and Down move one row. Page Up and Page Down move one page. Home and End move to the first
  and last rows.
- Right expands a collapsed row. On an expanded row, Right moves to the first child.
- Left collapses an expanded row. On another row, Left moves to the parent.
- Typed text moves to the next row whose text starts with that text. After a pause of one second,
  a new search starts.

A click selects a row. A click on the chevron also expands or collapses the row. Enter and a
double click call the handler from `set_on_activate`.

### Example

The program `examples/widgets/tree_view.cpp` shows a tree of folders and files. It moves to the
`docs` row, expands it, selects the file in it and opens the file.

```{.cpp include="examples/widgets/tree_view.cpp"}
```
