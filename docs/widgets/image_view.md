## Image view

An image view shows one picture. It centers the picture in its bounds. It scales the picture down
to fit and never makes it larger than its natural size.

![A gradient image](../images/widgets/image_view.png)

### Create and configure

Create the view and call `set_image` with a `raster_image_ptr`. Make an image from RGBA bytes
with `raster_image::create(width, height, pixels)`. Basalt does not decode image files. Decode
the file in your program first.

- `set_description` sets the text that assistive technology reads in place of the picture. Always
  set it.
- `set_maximum_height` limits the height that the view asks for.
- `image_bounds()` returns the rectangle of the picture inside the view. It is empty when the
  view has no image.

The natural size is one device pixel for each image pixel. On a display with a scale of 2, a 96
pixel image takes 48 layout pixels.

### Events

An image view does not handle input.

### Example

The program `examples/widgets/image_view.cpp` builds a gradient in memory, shows it with a maximum height, and prints the size that the picture takes.

```{.cpp include="examples/widgets/image_view.cpp"}
```
