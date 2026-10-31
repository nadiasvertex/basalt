# The widget library. HarfBuzz is private: no public header includes it.
add_library(basalt STATIC
  basalt/accessibility/role.cpp
  basalt/accessibility/tree.cpp
  basalt/color.cpp
  basalt/event_router.cpp
  basalt/focus.cpp
  basalt/focus_painter.cpp
  basalt/geometry.cpp
  basalt/icon.cpp
  basalt/image.cpp
  basalt/input.cpp
  basalt/units.cpp
  basalt/theme.cpp
  basalt/widget.cpp
  basalt/widget_painter.cpp
  basalt/widget_tree.cpp
  basalt/layout.cpp
  basalt/paint.cpp
  basalt/text/bidi.cpp
  basalt/text/font_face.cpp
  basalt/text/font_manager.cpp
  basalt/text/line_break.cpp
  basalt/text/paragraph.cpp
  basalt/text/text_layout.cpp
  basalt/text/text_shaper.cpp
  basalt/widgets/box.cpp
  basalt/widgets/button.cpp
  basalt/widgets/checkbox.cpp
  basalt/widgets/chip.cpp
  basalt/widgets/choice_control.cpp
  basalt/widgets/collection_view.cpp
  basalt/widgets/dialog.cpp
  basalt/widgets/diagnostics_list.cpp
  basalt/widgets/field_text.cpp
  basalt/widgets/grid_layout.cpp
  basalt/widgets/image_tile.cpp
  basalt/widgets/image_view.cpp
  basalt/widgets/label.cpp
  basalt/widgets/menu.cpp
  basalt/widgets/menu_bar.cpp
  basalt/widgets/overlay_host.cpp
  basalt/widgets/palette.cpp
  basalt/widgets/progress_bar.cpp
  basalt/widgets/radio_button.cpp
  basalt/widgets/scroll_bar.cpp
  basalt/widgets/scroll_view.cpp
  basalt/widgets/separator.cpp
  basalt/widgets/slider.cpp
  basalt/widgets/split_view.cpp
  basalt/widgets/splitter.cpp
  basalt/widgets/stack.cpp
  basalt/widgets/status_bar.cpp
  basalt/widgets/tabs.cpp
  basalt/widgets/text_field.cpp
  basalt/widgets/text_line.cpp
  basalt/widgets/toggle.cpp
  basalt/widgets/tree_view.cpp
  basalt/widgets/grid_view.cpp
  basalt/widgets/accordion.cpp
  basalt/widgets/avatar.cpp
  basalt/widgets/navigation_bar.cpp
  basalt/widgets/popover.cpp
  basalt/widgets/combo_box.cpp
  basalt/widgets/select.cpp
  basalt/widgets/tooltip.cpp
  basalt/widgets/uploader.cpp)
add_library(basalt::basalt ALIAS basalt)

target_compile_features(basalt PUBLIC cxx_std_23)
target_include_directories(basalt PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/generated>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(basalt PRIVATE $<BUILD_INTERFACE:basalt_compiler_options>)
set_target_properties(basalt PROPERTIES EXPORT_NAME basalt)

if(BASALT_HARFBUZZ STREQUAL "bundled")
  # The HarfBuzz objects go into libbasalt itself, so the installed package is one archive.
  target_sources(basalt PRIVATE $<TARGET_OBJECTS:basalt_harfbuzz>)
  target_include_directories(basalt SYSTEM PRIVATE
    $<BUILD_INTERFACE:${harfbuzz_SOURCE_DIR}/src>)
else()
  target_link_libraries(basalt PRIVATE harfbuzz::harfbuzz)
endif()
