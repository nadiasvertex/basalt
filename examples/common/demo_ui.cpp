#include "demo_ui.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace example {

namespace {

using namespace basalt;

auto gradient_image() -> raster_image_ptr {
  constexpr auto side = std::uint32_t{64};
  auto pixels = std::vector<std::uint8_t>{};
  pixels.reserve(std::size_t{side} * side * 4);
  for (auto y = std::uint32_t{0}; y < side; ++y) {
    for (auto x = std::uint32_t{0}; x < side; ++x) {
      pixels.insert(pixels.end(),
                    {static_cast<std::uint8_t>(x * 4), static_cast<std::uint8_t>(y * 4),
                     std::uint8_t{160}, std::uint8_t{255}});
    }
  }
  return raster_image::create(side, side, std::move(pixels));
}

auto lower_case(std::string text) -> std::string {
  std::ranges::transform(text, text.begin(),
                         [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return text;
}

auto palette_commands() -> std::vector<palette_entry> {
  return {
      {.id = "theme", .label = "Toggle theme", .detail = "Ctrl+T", .category = "View"},
      {.id = "dialog", .label = "Show dialog", .category = "Window"},
      {.id = "upload", .label = "Select files", .category = "File"},
      {.id = "quit", .label = "Quit", .detail = "Ctrl+Q", .category = "File", .dimmed = true},
  };
}

auto filtered_commands(std::string const &query) -> std::vector<palette_entry> {
  auto result = std::vector<palette_entry>{};
  auto const needle = lower_case(query);
  for (auto &entry : palette_commands()) {
    if (lower_case(entry.label).find(needle) != std::string::npos) {
      result.push_back(std::move(entry));
    }
  }
  return result;
}

// Adds a heading and a column for one group of widgets, and returns the column.
auto add_section(box &parent, std::string heading) -> box & {
  parent.add_child(std::make_unique<separator>());
  parent.add_child(std::make_unique<label>(std::move(heading), text_role::heading_small));
  auto &column = parent.emplace_child<box>(axis::vertical);
  column.set_gap(fixed(8));
  return column;
}

auto add_row(box &parent) -> box & {
  auto &row = parent.emplace_child<box>(axis::horizontal);
  row.set_gap(fixed(8));
  row.set_cross_alignment(alignment::center);
  return row;
}

auto add_fixed_height(box &parent, std::unique_ptr<widget> child, float height) -> void {
  parent.add(std::move(child), box_item{.basis = fixed(height)});
}

auto build_bars(box &page, demo_state &state) -> void {
  auto &navigation = page.emplace_child<navigation_bar>("Main navigation");
  navigation.set_title("Basalt");
  navigation.emplace_leading<button>("Menu").set_variant(button_variant::tertiary);
  navigation.add_section("home", "Home");
  navigation.add_section("docs", "Docs").add_entry("api", "API reference");
  navigation.add_utility_link("help", "Help");
  navigation.emplace_center<label>("Widget gallery").set_tone(text_tone::subdued);
  navigation.set_avatar(std::make_unique<avatar>("Ada Lovelace", control_size::small));
  navigation.select("home");
  navigation.set_on_navigate([&state](std::string const &id) {
    state.status->set_section_text("hint", "Navigated to " + id);
  });

  auto &menus = page.emplace_child<menu_bar>();
  menus.add_menu("File", [](menu &m) {
    m.add_item("new", "New", "Ctrl+N");
    m.add_item("open", "Open", "Ctrl+O");
    m.add_separator();
    m.add_item("quit", "Quit", "Ctrl+Q");
  });
  menus.add_menu("View", [&state](menu &m) { m.add_item("theme", "Dark theme", "", state.dark); });
  menus.set_on_command([&state](std::string const &id) {
    if (id == "quit") {
      state.running = false;
    }
    state.status->set_section_text("hint", "Menu command: " + id);
  });
}

auto build_text_section(box &page, demo_state &state) -> void {
  auto &section = add_section(page, "Text");
  auto &heading = section.emplace_child<label>("Heading", text_role::heading_medium);
  heading.set_heading_level(2);
  auto &tones = add_row(section);
  tones.emplace_child<label>("Normal");
  tones.emplace_child<label>("Subdued").set_tone(text_tone::subdued);
  tones.emplace_child<label>("Interactive").set_tone(text_tone::interactive);
  tones.emplace_child<label>("Danger").set_tone(text_tone::danger);
  tones.emplace_child<label>("Warning").set_tone(text_tone::warning);
  tones.emplace_child<label>("Success").set_tone(text_tone::success);

  auto &field = section.emplace_child<text_field>();
  field.set_placeholder("Type here");
  field.set_accessible_name("Demo text");
  state.echo_label = &section.emplace_child<label>("You typed: ");
  field.set_on_change(
      [&state](std::string const &text) { state.echo_label->set_text("You typed: " + text); });

  auto &notes = section.emplace_child<text_field>("A multi-line field wraps at its width.", true);
  notes.set_accessible_name("Notes");
  auto &read_only = section.emplace_child<text_field>("Read-only text");
  read_only.set_read_only(true);
  section.emplace_child<tooltip>("A tooltip bubble");
}

auto build_button_section(box &page, overlay_host &host, widget_tree &tree, demo_state &state)
    -> void {
  auto &section = add_section(page, "Buttons, progress and overlays");
  auto &buttons = add_row(section);

  auto &count = buttons.emplace_child<button>("Count a click");
  count.set_variant(button_variant::primary);
  state.click_label = &buttons.emplace_child<label>("Clicks: 0");
  count.set_on_press(
      [&state] { state.click_label->set_text(std::format("Clicks: {}", ++state.clicks)); });

  state.work_button = &buttons.emplace_child<button>("Start work");
  state.work_button->set_on_press([&state] {
    state.working = !state.working;
    state.work_button->set_label(state.working ? "Stop work" : "Start work");
    state.progress->set_fraction(state.working ? std::nullopt : std::optional<float>{0});
  });

  auto &theme_button = buttons.emplace_child<button>("Toggle theme");
  theme_button.set_variant(button_variant::tertiary);
  theme_button.set_shortcut("Ctrl+T");
  theme_button.set_on_press([&state, &tree] {
    state.dark = !state.dark;
    tree.set_theme(make_theme(state.dark ? color_scheme::dark : color_scheme::light));
  });

  auto &quit = buttons.emplace_child<button>("Quit");
  quit.set_variant(button_variant::danger);
  quit.set_on_press([&state] { state.running = false; });

  state.progress = &section.emplace_child<progress_bar>(std::optional<float>{0});
  section.emplace_child<progress_bar>(std::optional<float>{0.6F});

  auto &overlays = add_row(section);
  overlays.emplace_child<button>("Show dialog", [&host] {
    auto shown = std::make_unique<dialog>(
        "Discard changes?", "The demo has no changes to discard.",
        std::vector<dialog_button>{{"Cancel"}, {"Discard", button_variant::danger}}, 0);
    auto &window = *shown;
    auto const id = host.show_dialog(std::move(shown));
    window.set_on_choice([&host, id](std::size_t) { host.dismiss(id); });
  });

  overlays.emplace_child<button>("Command palette", [&host, &state] {
    auto shown = std::make_unique<palette>();
    auto &window = *shown;
    window.set_placeholder("Type a command");
    window.set_entries(palette_commands());
    auto const id = host.show_dialog(std::move(shown));
    window.set_on_query_change(
        [&window](std::string const &query) { window.set_entries(filtered_commands(query)); });
    window.set_on_choose([&host, &state, id](std::string const &command) {
      state.status->set_section_text("hint", "Palette command: " + command);
      host.dismiss(id);
    });
    window.focus_query();
  });

  auto &details = overlays.emplace_child<popover>(popover_trigger::click);
  details.emplace_trigger<button>("Click popover").set_on_press([&details] { details.toggle(); });
  details.set_text("Popover content that opens on click.");

  auto &hint = overlays.emplace_child<popover>(popover_trigger::hover);
  hint.emplace_trigger<label>("Hover popover").set_tone(text_tone::interactive);
  hint.set_text("Shown after the pointer rests.");
}

auto build_choice_section(box &page) -> void {
  auto &section = add_section(page, "Choices");
  auto &row = add_row(section);
  row.set_cross_alignment(alignment::start);
  row.set_gap(fixed(32));

  auto &checks = row.emplace_child<checkbox_group>("Toppings");
  checks.enable_select_all("All toppings");
  checks.add_option("Cheese", true);
  checks.add_option("Mushrooms");
  checks.add_option("Olives");

  auto &single = row.emplace_child<checkbox>("Needs review", accessibility::check_state::mixed);
  single.set_helper_text("Mixed state with helper text");

  auto &radios = row.emplace_child<radio_group>("Size");
  radios.add_option("Small");
  radios.add_option("Medium");
  radios.add_option("Large");
  radios.select(1);

  auto &switches = row.emplace_child<box>(axis::vertical);
  switches.set_gap(fixed(8));
  auto &wifi = switches.emplace_child<toggle>("Wi-Fi", true);
  wifi.set_status_text("Off", "On");
  switches.emplace_child<toggle>("Bluetooth");

  auto &lists = add_row(section);
  lists.set_cross_alignment(alignment::start);
  lists.set_gap(fixed(16));
  auto &one = lists.emplace_child<select>("Language");
  one.set_placeholder("Choose one");
  for (auto const *name : {"C++", "C#", "Go"}) {
    one.add_option(name);
  }
  auto &many = lists.emplace_child<select>("Editors");
  many.set_mode(select_mode::multiple);
  many.set_placeholder("Choose several");
  for (auto const *name : {"Vim", "Emacs", "Helix"}) {
    many.add_option(name);
  }
}

auto build_chip_section(box &page) -> void {
  auto &section = add_section(page, "Chips and avatars");
  auto &filters = section.emplace_child<chip_group>("Filters", chip_selection::multiple);
  filters.add_chip("Open").set_selectable(true);
  filters.add_chip("Closed").set_selectable(true);
  filters.add_chip("Removable").set_removable(true);
  filters.set_remove_all_label("Remove all");

  auto &states = add_row(section);
  states.emplace_child<chip>("Error").set_error(true);
  states.emplace_child<chip>("Loading").set_loading(true);
  states.emplace_child<chip>("Small", control_size::small);

  auto &people = add_row(section);
  auto &named = people.emplace_child<avatar>("Grace Hopper");
  named.set_supporting_text("Rear admiral");
  auto &leading = people.emplace_child<avatar>("Alan Turing", control_size::small);
  leading.set_layout(avatar_layout::leading);
  people.emplace_child<avatar>("Katherine Johnson", control_size::large)
      .set_layout(avatar_layout::graphic_only);
  auto &photo = people.emplace_child<avatar>("Photo", control_size::large);
  photo.set_image(gradient_image());
  photo.set_layout(avatar_layout::graphic_only);

  auto &badged = people.emplace_child<stack>();
  badged.emplace_child<avatar>("Margaret Hamilton", control_size::large)
      .set_layout(avatar_layout::graphic_only);
  badged.add(std::make_unique<chip>("New", control_size::small),
             stack_item{.horizontal = alignment::end, .vertical = alignment::start});
}

auto build_structure_section(box &page) -> void {
  auto &section = add_section(page, "Structure");

  auto &accordion_group = section.emplace_child<accordion>("Details");
  accordion_group.set_appearance(accordion_appearance::outlined);
  accordion_group.add_item("First item", std::make_unique<label>("Content of the first item."))
      .set_expanded(true);
  accordion_group.add_item("Second item", std::make_unique<label>("Content of the second item."));

  auto &tabs_bar = section.emplace_child<tab_bar>("Sections");
  tabs_bar.add_tab("Overview");
  tabs_bar.add_tab("Activity");
  tabs_bar.add_tab("Settings").set_closable(true);
  tabs_bar.select(0);

  auto split = std::make_unique<split_view>(axis::horizontal);
  split->set_first(std::make_unique<label>("First pane"));
  split->set_second(std::make_unique<label>("Second pane"));
  split->set_first_length(fixed(200));
  split->set_resizable(true);
  add_fixed_height(section, std::move(split), 64);

  auto &horizontal_bar = section.add(std::make_unique<scroll_bar>(axis::horizontal),
                                     box_item{.basis = scroll_bar::default_thickness});
  horizontal_bar.set_metrics(100, 25, 0);
  horizontal_bar.set_on_scroll(
      [&horizontal_bar](float offset) { horizontal_bar.set_metrics(100, 25, offset); });

  auto scroller = std::make_unique<scroll_view>(scroll_axes::vertical);
  scroller->set_scrollbars(true);
  auto &rows = scroller->emplace_content<box>(axis::vertical);
  for (auto index = 1; index <= 12; ++index) {
    rows.emplace_child<label>(std::format("Scrolled row {}", index));
  }
  add_fixed_height(section, std::move(scroller), 96);

  auto &picture = section.emplace_child<image_view>();
  picture.set_image(gradient_image());
  picture.set_description("A color gradient");
  picture.set_maximum_height(fixed(64));
}

auto build_data_section(box &page, demo_state &state) -> void {
  auto &section = add_section(page, "Diagnostics and files");
  auto list = std::make_unique<diagnostics_list>();
  list->set_entries({
      {diagnostic_severity::error, "Missing closing tag", "document.xml:12:5"},
      {diagnostic_severity::warning, "Unused style", "styles.xml:3:1"},
      {diagnostic_severity::info, "Document saved", ""},
  });
  list->set_on_activate([&state](std::size_t, diagnostic_entry const &entry) {
    state.status->set_section_text("hint", "Go to " + entry.location);
  });
  section.add_child(std::move(list));

  auto &files = section.emplace_child<uploader>("Attachments");
  files.set_prompt("Drop files here");
  files.set_accepted_suffixes({".png", ".txt"});
  files.add_files({"notes.txt", "photo.png"});
}

} // namespace

auto build_demo_ui(basalt::widget_tree &tree, demo_state &state) -> void {
  using namespace basalt;

  auto &host = tree.emplace_root<overlay_host>();
  auto &window = host.set_content(std::make_unique<box>(axis::vertical));

  build_bars(window, state);

  auto &scroller = window.add(std::make_unique<scroll_view>(scroll_axes::vertical),
                              box_item{.grow = 1, .shrink = 0, .basis = fixed(0)});
  scroller.set_scrollbars(true);
  scroller.set_focus_policy(focus_policy::tab);
  auto &page = scroller.emplace_content<box>(axis::vertical);
  page.set_padding(length_insets::uniform(fixed(24)));
  page.set_gap(fixed(16));

  page.add_child(std::make_unique<label>("Basalt on SDL and Skia", text_role::heading_large));
  page.emplace_child<label>(
          "Widgets, layout and input come from Basalt. SDL owns the window; Skia draws the pixels.")
      .set_tone(text_tone::subdued);

  build_text_section(page, state);
  build_button_section(page, host, tree, state);
  build_choice_section(page);
  build_chip_section(page);
  build_structure_section(page);
  build_data_section(page, state);

  state.status = &window.emplace_child<status_bar>();
  state.status->add_section("renderer", "", 1);
  state.status->add_section("hint", "Tab moves focus; Space or Enter presses a button");
}

auto load_demo_fonts(basalt::text::font_manager &fonts) -> bool {
  auto const directory = std::filesystem::path{BASALT_EXAMPLE_FONT_DIR};
  auto added = std::size_t{0};
  for (auto const *name :
       {"NotoSans-VariableFont_wdth,wght.ttf", "NotoSans-Italic-VariableFont_wdth,wght.ttf"}) {
    added += fonts.add_file(directory / name);
  }
  // System fonts supply the monospace family and scripts Noto Sans lacks.
  fonts.add_system_fonts();
  return added > 0;
}

} // namespace example
