#include "demo_ui.h"

#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <string>

namespace example {

auto build_demo_ui(basalt::widget_tree &tree, demo_state &state) -> void {
  using namespace basalt;

  auto &root = tree.emplace_root<box>(axis::vertical);
  root.set_padding(length_insets::uniform(fixed(24)));
  root.set_gap(fixed(16));

  root.add_child(std::make_unique<label>("Basalt on SDL and Skia", text_role::heading_large));
  auto &intro = root.emplace_child<label>(
      "Widgets, layout and input come from Basalt. SDL owns the window; Skia draws the pixels.");
  intro.set_tone(text_tone::subdued);

  auto &field = root.emplace_child<text_field>();
  field.set_placeholder("Type here");
  field.set_accessible_name("Demo text");
  state.echo_label = &root.emplace_child<label>("You typed: ");
  field.set_on_change(
      [&state](std::string const &text) { state.echo_label->set_text("You typed: " + text); });

  root.add_child(std::make_unique<separator>());

  auto &buttons = root.emplace_child<box>(axis::horizontal);
  buttons.set_gap(fixed(8));
  buttons.set_cross_alignment(alignment::center);

  auto &count = buttons.emplace_child<button>("Count a click");
  count.set_variant(button_variant::primary);
  state.click_label = &buttons.emplace_child<label>("Clicks: 0");
  count.set_on_press([&state] {
    state.click_label->set_text(std::format("Clicks: {}", ++state.clicks));
  });

  state.work_button = &buttons.emplace_child<button>("Start work");
  state.work_button->set_on_press([&state] {
    state.working = !state.working;
    state.work_button->set_label(state.working ? "Stop work" : "Start work");
    state.progress->set_fraction(state.working ? std::nullopt : std::optional<float>{0});
  });

  auto &theme_button = buttons.emplace_child<button>("Toggle theme");
  theme_button.set_variant(button_variant::tertiary);
  theme_button.set_on_press([&state, &tree] {
    state.dark = !state.dark;
    tree.set_theme(make_theme(state.dark ? color_scheme::dark : color_scheme::light));
  });

  auto &quit = buttons.emplace_child<button>("Quit");
  quit.set_variant(button_variant::danger);
  quit.set_on_press([&state] { state.running = false; });

  state.progress = &root.emplace_child<progress_bar>(std::optional<float>{0});

  // Takes the space left over so the status bar sits at the bottom edge.
  root.add(std::make_unique<box>(), box_item{.grow = 1});

  state.status = &root.emplace_child<status_bar>();
  state.status->add_section("renderer", "", 1);
  state.status->add_section("hint", "Tab moves focus; Space or Enter presses a button");
}

auto load_demo_fonts(basalt::text::font_manager &fonts) -> bool {
  auto const directory = std::filesystem::path{BASALT_EXAMPLE_FONT_DIR};
  auto added = std::size_t{0};
  for (auto const *name : {"NotoSans-VariableFont_wdth,wght.ttf",
                           "NotoSans-Italic-VariableFont_wdth,wght.ttf"}) {
    added += fonts.add_file(directory / name);
  }
  // System fonts supply the monospace family and scripts Noto Sans lacks.
  fonts.add_system_fonts();
  return added > 0;
}

} // namespace example
