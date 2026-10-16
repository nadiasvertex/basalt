## Split view

A split view has two panes with a divider between them. With `axis::horizontal` the panes sit side
by side. With `axis::vertical` they are stacked. The user can drag the divider when the view is
resizable.

![A split view with a files pane and an editor pane](../images/widgets/split_view.png)

### Create and configure

Pass an `axis` to the constructor. Set the panes with `set_first` and `set_second`. When only one
pane is present, it fills the view and there is no divider.

Choose what stays constant when the view changes size:

| Call | Anchor | Meaning |
| ---- | ------ | ------- |
| `set_ratio(r)` | `proportional` | The first pane keeps the fraction `r` of the space. |
| `set_first_length(length)` | `first` | The first pane keeps its length. |
| `set_second_length(length)` | `second` | The second pane keeps its length. |

- `set_minimums(first, second)` sets the smallest pane lengths. The view enforces them on every
  layout. When both do not fit, the first minimum wins.
- `set_divider_thickness` sets the thickness of the visible line.
- `move_divider_to(position)` and `move_divider_by(delta)` move the divider. They return the
  position after the minimums apply.
- `divider_rect()` returns the divider as of the last layout.

### Events

`set_resizable(true)` adds a `splitter` over the divider. `handle()` returns it. The splitter is
wider than the visible line, so that the user can grab it. A pointer press on it starts a drag. The
splitter is a Tab stop. The arrow keys move the divider by `splitter::key_step`. Home and End move
it to the limits.

You do not create a `splitter` yourself.

### Example

The program `examples/widgets/split_view.cpp` makes a resizable view with a first pane of fixed length. It moves the divider by one step and prints the position before and after.

```{.cpp include="examples/widgets/split_view.cpp"}
```
