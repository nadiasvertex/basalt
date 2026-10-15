add_executable(basalt_tests
  basalt/accessibility_test.cpp
  basalt/input_probe.h
  basalt/leaf.h
  basalt/color_test.cpp
  basalt/event_router_test.cpp
  basalt/focus_test.cpp
  basalt/geometry_test.cpp
  basalt/paint_test.cpp
  basalt/input_test.cpp
  basalt/units_test.cpp
  basalt/theme_test.cpp
  basalt/widget_test.cpp
  basalt/widget_tree_test.cpp
  basalt/layout_test.cpp
  basalt/box_test.cpp
  basalt/stack_test.cpp
  basalt/scroll_view_test.cpp
  basalt/split_view_test.cpp
  basalt/ui_fixture.h
  basalt/widget_painter_test.cpp
  basalt/label_test.cpp
  basalt/button_test.cpp
  basalt/toggle_test.cpp
  basalt/checkbox_test.cpp
  basalt/radio_button_test.cpp
  basalt/menu_test.cpp
  basalt/tabs_test.cpp
  basalt/text_field_test.cpp
  basalt/scroll_controls_test.cpp
  basalt/splitter_test.cpp
  basalt/status_bar_test.cpp
  basalt/diagnostics_list_test.cpp
  basalt/image_view_test.cpp
  basalt/palette_test.cpp
  basalt/progress_bar_test.cpp
  basalt/text/bidi_test.cpp
  basalt/text/font_manager_test.cpp
  basalt/text/font_support.h
  basalt/text/line_break_test.cpp
  basalt/text/paragraph_test.cpp
  basalt/text/text_layout_test.cpp
  basalt/text/text_shaper_test.cpp)
target_link_libraries(basalt_tests PRIVATE basalt::basalt Catch2::Catch2WithMain
  basalt_compiler_options)
target_compile_definitions(basalt_tests PRIVATE
  BASALT_TEST_FONT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/fonts")
catch_discover_tests(basalt_tests PROPERTIES LABELS widgets)

add_test(NAME include_boundaries
  COMMAND ${CMAKE_COMMAND} -DBASALT_SOURCE_DIR=${PROJECT_SOURCE_DIR}
    -P ${PROJECT_SOURCE_DIR}/cmake/check_include_boundaries.cmake)
set_tests_properties(include_boundaries PROPERTIES LABELS boundaries)
