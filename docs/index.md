# Overview

![Basalt logo](basalt-logo-light.png)

Basalt is a C++23 widget library. It follows the Cubit design system. Basalt turns a widget tree
into paint commands and accessibility snapshots. Your application supplies the window, the
renderer, and the input events.

This guide has three parts. This overview describes what Basalt does. The architecture chapter
describes how the parts fit together. The getting-started chapter shows how to build a first
program.

## Who this guide is for

This guide is for C++ programmers. You must know C++20 or later and CMake. You do not need to
know Cubit, HarfBuzz, or any GUI library.

## What Basalt does

Basalt does these tasks:

- It keeps a tree of widgets and lays them out.
- It routes input events to widgets and manages keyboard focus.
- It records what to draw as a list of paint commands.
- It shapes and lays out Unicode text, including bidirectional text and line breaks.
- It builds a semantic tree for assistive technology.
- It supplies Cubit colors, sizes, and text styles as a theme.

## What Basalt does not do

Basalt does not do these tasks. Your application, called the *host* in this guide, does them.

- It does not create windows.
- It does not draw pixels. It gives your renderer a list of paint commands.
- It does not read keyboard, mouse, or touch input from the operating system.
- It does not talk to a platform accessibility API. It gives you a snapshot to send to one.
- It does not open file dialogs. The host supplies a function that does.

This split makes Basalt portable. The same widget tree works with SDL and Skia, with a GPU
renderer, or with a test that has no window.

## Layers

The library has three layers.

| Layer | Contents |
| ----- | -------- |
| Core | Geometry, units, color, theme, widget tree, layout, events, focus, accessibility, paint commands |
| Text | Font discovery, fallback, shaping, bidirectional text, line breaking, paragraphs |
| Widgets | Box, label, button, text field, menu, tabs, dialog, and the other Cubit widgets |

The header `basalt/basalt.h` includes every public header.

## Requirements

Basalt needs CMake 3.25 or later. It needs a C++23 compiler and standard library that support
`std::expected` and `<print>`. The `README.md` file lists the tested compilers and the build
options.

## Next steps

- Read [Architecture](architecture.md) to learn the main types and the frame cycle.
- Read [Getting Started](getting-started.md) to build a program.
- Read the headers in `include/basalt/`. Each class has a comment that states its rules.
- Read `examples/sdl_skia` for a complete host.
