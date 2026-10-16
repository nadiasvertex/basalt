## Scroll bar

A scroll bar is a track with a thumb. It does not scroll anything. It reports the offset that the
user asks for, and its owner applies it. A `scroll_view` can create scroll bars for you. Create one
yourself only to scroll custom content.

![A horizontal scroll bar](../images/widgets/scroll_bar.png)

### Create and configure

Pass an `axis` to the constructor. Call `set_metrics(content, viewport, offset)` with the full
scrollable length, the visible length and the start of the visible part. All values are in layout
pixels along the axis.

- `thumb_rect()` returns the thumb rectangle. It is empty when nothing scrolls.
- `max_offset()` returns the largest offset.
- `scroll_bar::default_thickness` is the usual thickness of a bar.

### Events

`set_on_scroll` calls its handler with the offset that the user asks for. Basalt clamps the offset
to 0 and `max_offset()`. In the handler, apply the offset and call `set_metrics` again.

The user drags the thumb, or presses the track beside it to move one page toward the press. The bar
is not a Tab stop. The scrolled content handles keys. Assistive technology uses the increment and
decrement actions.

### Example

The program `examples/widgets/scroll_bar.cpp` creates a horizontal bar and presses the track after the thumb. It prints the offset that the bar asks for.

```{.cpp include="examples/widgets/scroll_bar.cpp"}
```
