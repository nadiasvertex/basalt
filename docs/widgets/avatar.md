## Avatar

An avatar is a circle that stands for a person or an entity. It shows a photo when you give it
one. Otherwise it shows the initials of the name. The name and a line of supporting text can sit
beside the circle.

![Three avatars with different layouts](../images/widgets/avatar.png)

### Create and configure

Pass a name and an optional `control_size` to the constructor.

- `set_supporting_text` adds a second line of text, such as a role.
- `set_image` sets a photo. The photo fills the circle.
- `set_layout` picks `trailing` (text after the circle), `leading` (text before it) or
  `graphic_only`. A graphic-only avatar shows the name in a tooltip.
- `diameter()` returns the diameter of the circle in layout pixels.

`avatar_initials(name)` returns the initials that the avatar shows. It takes the first letter of
the first and the last word. A single word gives one letter.

### Events

An avatar does not handle input. A graphic-only avatar shows its tooltip when the pointer rests
on it or when it has keyboard focus.

### Example

The program `examples/widgets/avatar.cpp` shows an avatar with supporting text, a small leading avatar and a large graphic-only avatar. It prints the initials and the diameter.

```{.cpp include="examples/widgets/avatar.cpp"}
```
