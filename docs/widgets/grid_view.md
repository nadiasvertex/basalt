## Grid view

A grid view shows many items as a grid of square cells, for example a wall of thumbnails. The grid
has no child widgets. It paints only the cells that you can see. Thus a grid of a million items
costs about the same as a grid of a hundred.

### Create and configure

Create a `grid_view` with an accessible name, and put it in a vertical `scroll_view`. The grid
takes the width of the view, and it is as tall as its rows.

- `set_item_count` sets the number of items.
- `set_cell_size` sets the side of each cell, in layout pixels.
- `set_gap` sets the space between the cells and around them. The first cell starts at
  (gap, gap).
- `set_cell_painter` sets the function that paints one item. The function gets the render context,
  the index of the item and the rectangle of its cell, in the coordinates of the grid.

The grid fits as many columns into its width as it can, and at least one. Items go from left to
right, then from top to bottom.

These functions give the layout:

- `columns()`, `rows()` and `cell_rect(index)` give the current layout.
- `visible_range()` gives the items whose rows touch the visible part of the grid.
- `visible_area()` gives the visible part of the grid. The grid calculates it from the bounds of
  its ancestors and the window.
- `index_at(point)` gives the item under a point. It gives nothing for a point in a gap.
- `nearest_index(point)` gives the item under a point or in the gaps after it, clamped into the
  grid. Use it as the anchor of a zoom.
- `indices_in(rect)` gives each item whose cell touches a rectangle. Use it for selection with a
  rectangle.

The grid keeps no copy of your data. When the data of one item changes, call
`invalidate_cell(index)`. The grid repaints only that cell, and only when it is visible.

Assistive technology sees the grid as one list, with its name and the number of items. The items
are not widgets, so assistive technology cannot reach each item.

### Events

The grid calls the handler from `set_on_visible_range_changed` when other items come into view,
after a scroll or a layout. Use it to load the data of those items.

A grid view does not handle input. Your program keeps the selection and the item data. To add
selection or zoom, make a subclass and override `on_event`. Pointer positions in `on_event` are in
the coordinates of the grid, which `index_at` and `indices_in` use.

Basalt geometry uses `float`. A grid that is taller than about 16 million layout pixels loses
precision.

### Example

The program `examples/widgets/grid_view.cpp` shows ten thousand colored cells in a scroll view. It
scrolls down, prints the items that come into view, and finds the item under a point.

```{.cpp include="examples/widgets/grid_view.cpp"}
```
