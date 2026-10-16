#include "example_host.h"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <print>
#include <stdexcept>
#include <vector>

#include "skia_painter.h"

namespace example {

namespace {

constexpr auto screenshot_scale = 2.0F;
constexpr auto frame_step = std::chrono::nanoseconds{16'666'667};

auto write_pam(char const *path, pixel_view const &pixels) -> bool {
  auto const file =
      std::unique_ptr<std::FILE, decltype(&std::fclose)>{std::fopen(path, "wb"), &std::fclose};
  if (file == nullptr || pixels.data == nullptr) {
    return false;
  }
  std::print(file.get(),
             "P7\nWIDTH {}\nHEIGHT {}\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n",
             pixels.width, pixels.height);
  auto row = std::vector<std::uint8_t>(static_cast<std::size_t>(pixels.width) * 4);
  for (auto y = 0; y < pixels.height; ++y) {
    auto const *source = static_cast<std::uint8_t const *>(pixels.data) + y * pixels.row_bytes;
    for (auto x = 0; x < pixels.width; ++x) {
      // Skia drew BGRA onto an opaque background, so the color needs no unpremultiplying.
      row[x * 4 + 0] = source[x * 4 + 2];
      row[x * 4 + 1] = source[x * 4 + 1];
      row[x * 4 + 2] = source[x * 4 + 0];
      row[x * 4 + 3] = source[x * 4 + 3];
    }
    if (std::fwrite(row.data(), 1, row.size(), file.get()) != row.size()) {
      return false;
    }
  }
  return true;
}

} // namespace

struct example_host::impl {
  explicit impl(basalt::size viewport)
      : viewport(viewport), tree(basalt::make_theme(basalt::color_scheme::light)), router(tree) {
    auto const directory = std::filesystem::path{BASALT_EXAMPLE_FONT_DIR};
    auto added = std::size_t{0};
    for (auto const *name :
         {"NotoSans-VariableFont_wdth,wght.ttf", "NotoSans-Italic-VariableFont_wdth,wght.ttf"}) {
      added += fonts.add_file(directory / name);
    }
    if (added == 0) {
      throw std::runtime_error{"Cannot load the Noto Sans fonts from " + directory.string()};
    }
    shaper = std::make_unique<basalt::text::text_shaper>(fonts);
    tree.set_text_shaper(shaper.get());
    tree.set_viewport(viewport);

    host = &tree.emplace_root<basalt::overlay_host>();
    page = &host->set_content(std::make_unique<basalt::box>(basalt::axis::vertical));
    page->set_padding(basalt::length_insets::uniform(basalt::fixed(16)));
    page->set_gap(basalt::fixed(12));
    page->set_cross_alignment(basalt::alignment::start);
  }

  auto frame() -> void {
    tree.update_layout();
    router.update_hover();
  }

  auto send(basalt::event const &e) -> void {
    router.dispatch(e);
    frame();
  }

  auto pointer(basalt::pointer_action action, basalt::point at) -> void {
    send(basalt::pointer_event{
        .action = action,
        .position = at,
        .button = action == basalt::pointer_action::move ? basalt::pointer_button::none
                                                         : basalt::pointer_button::primary,
        .buttons = action == basalt::pointer_action::down ? basalt::pointer_button::primary
                                                          : basalt::pointer_button::none,
    });
  }

  basalt::size viewport;
  basalt::text::font_manager fonts;
  std::unique_ptr<basalt::text::text_shaper> shaper;
  basalt::widget_tree tree;
  basalt::event_router router;
  basalt::overlay_host *host = nullptr;
  basalt::box *page = nullptr;
  std::chrono::nanoseconds now{};
};

example_host::example_host(basalt::size viewport) : impl_(std::make_unique<impl>(viewport)) {}
example_host::~example_host() = default;

auto example_host::tree() -> basalt::widget_tree & { return impl_->tree; }
auto example_host::content() -> basalt::box & { return *impl_->page; }
auto example_host::overlays() -> basalt::overlay_host & { return *impl_->host; }
auto example_host::frame() -> void { impl_->frame(); }

auto example_host::advance(std::chrono::milliseconds duration) -> void {
  for (auto spent = std::chrono::nanoseconds{}; spent < duration; spent += frame_step) {
    impl_->now += frame_step;
    impl_->tree.advance_animations(impl_->now);
    impl_->frame();
  }
}

auto example_host::move_pointer_to(basalt::widget const &target) -> void {
  impl_->frame();
  impl_->pointer(basalt::pointer_action::move, target.bounds_in_root().center());
}

auto example_host::click_at(basalt::point at) -> void {
  impl_->frame();
  impl_->pointer(basalt::pointer_action::down, at);
  impl_->pointer(basalt::pointer_action::up, at);
}

auto example_host::click(basalt::widget const &target) -> void {
  impl_->frame();
  click_at(target.bounds_in_root().center());
}

auto example_host::press_key(basalt::key code, basalt::modifier modifiers) -> void {
  impl_->send(basalt::key_event{.code = code, .pressed = true, .modifiers = modifiers});
  impl_->send(basalt::key_event{.code = code, .pressed = false, .modifiers = modifiers});
}

auto example_host::type(std::string text) -> void {
  impl_->send(basalt::text_input_event{std::move(text)});
}

auto example_host::finish(int argc, char **argv) -> int {
  if (argc < 2) {
    return 0;
  }
  impl_->send(basalt::pointer_event{.action = basalt::pointer_action::leave});
  auto commands = basalt::paint_commands{};
  basalt::paint_tree(impl_->tree, commands);
  auto const width = static_cast<int>(impl_->viewport.width * screenshot_scale);
  auto const height = static_cast<int>(impl_->viewport.height * screenshot_scale);
  auto painter = skia_painter{};
  painter.render(commands, width, height, screenshot_scale,
                 impl_->tree.theme().colors.background_default);
  if (!write_pam(argv[1], painter.pixels())) {
    std::println(stderr, "Cannot write {}", argv[1]);
    return 1;
  }
  return 0;
}

} // namespace example
