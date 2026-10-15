# Dependencies of the example, pinned by full commit hash. SDL is only fetched when no installed
# SDL 3 is found. Set FETCHCONTENT_SOURCE_DIR_<NAME> to use a local checkout instead.
include_guard(GLOBAL)

# release-3.2.20
set(EXAMPLE_SDL3_URL "https://github.com/libsdl-org/SDL.git")
set(EXAMPLE_SDL3_COMMIT "96292a5b464258a2b926e0a3d72f8b98c2a81aa6")

# chrome/m138
set(EXAMPLE_SKIA_URL "https://skia.googlesource.com/skia.git")
set(EXAMPLE_SKIA_COMMIT "9d9ac365fe4916c31b31935a062b869d097324c4")
