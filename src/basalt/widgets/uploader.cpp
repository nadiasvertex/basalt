#include "basalt/widgets/uploader.h"

#include "basalt/event_router.h"
#include "basalt/widget_tree.h"

#include <algorithm>
#include <cctype>
#include <variant>

namespace basalt {

namespace a11y = accessibility;

namespace {

constexpr length section_gap = fixed(8);

auto file_name_of(std::string_view path) -> std::string {
  auto const slash = path.find_last_of("/\\");
  return std::string{slash == std::string_view::npos ? path : path.substr(slash + 1)};
}

auto lower(std::string_view text) -> std::string {
  std::string out{text};
  std::ranges::transform(out, out.begin(),
                         [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

} // namespace

struct uploader::plan {
  basalt::size size;
  rect zone;
  point button;
  point prompt;
  point list;
  point error;
  basalt::size list_size;
  basalt::size button_size;
};

uploader::uploader(std::string name, uploader_layout layout, uploader_behavior behavior)
    : name_(std::move(name)), layout_(layout), behavior_(behavior),
      prompt_("Drag and drop files here", text_role::body), error_({}, text_role::small),
      alive_(std::make_shared<uploader *>(this)) {
  select_ = &emplace_child<button>("Select Files", [this] { (void)select_files(); });
  list_ = &emplace_child<chip_group>("Selected files");
  list_->set_visible(false);
}

uploader::~uploader() { *alive_ = nullptr; }

auto uploader::set_layout(uploader_layout layout) -> void {
  if (layout != layout_) {
    layout_ = layout;
    if (layout_ == uploader_layout::no_drop_zone) {
      dragging_ = false;
    }
    invalidate_layout();
    invalidate_paint();
  }
}

auto uploader::set_behavior(uploader_behavior behavior) -> void { behavior_ = behavior; }

auto uploader::set_prompt(std::string text) -> void {
  if (prompt_.set_text(std::move(text))) {
    invalidate_layout();
    invalidate_paint();
  }
}

auto uploader::set_button_label(std::string label) -> void { select_->set_label(std::move(label)); }

auto uploader::set_accepted_suffixes(std::vector<std::string> suffixes) -> void {
  suffixes_.clear();
  for (std::string const &s : suffixes) {
    suffixes_.push_back(lower(s));
  }
}

auto uploader::accepts(std::string_view path) const -> bool {
  if (suffixes_.empty()) {
    return true;
  }
  std::string const name = lower(path);
  return std::ranges::any_of(suffixes_, [&](std::string const &suffix) {
    return name.size() >= suffix.size() &&
           name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0;
  });
}

auto uploader::files() const -> std::vector<std::string> {
  std::vector<std::string> out;
  for (entry const &e : entries_) {
    out.push_back(e.path);
  }
  return out;
}

auto uploader::add_files(std::vector<std::string> paths) -> std::vector<std::string> {
  std::vector<std::string> taken;
  std::string rejected;
  for (std::string &path : paths) {
    if (!accepts(path)) {
      rejected = file_name_of(path);
      continue;
    }
    if (std::ranges::any_of(entries_, [&](entry const &e) { return e.path == path; }) ||
        std::ranges::contains(taken, path)) {
      continue;
    }
    taken.push_back(std::move(path));
  }
  if (behavior_ == uploader_behavior::deferred_single && taken.size() > 1) {
    taken.resize(1);
  }
  if (!rejected.empty()) {
    set_error_message(rejected + " is not an accepted file type.");
  } else if (!taken.empty()) {
    set_error_message({});
  }
  if (taken.empty()) {
    return taken;
  }

  if (behavior_ == uploader_behavior::deferred_single) {
    clear();
  }
  for (std::string const &path : taken) {
    std::uint64_t const key = next_key_++;
    chip &shown = list_->add_chip(file_name_of(path));
    shown.set_removable(true);
    shown.set_loading(behavior_ == uploader_behavior::immediate);
    shown.set_on_remove([this, key] { removed(key); });
    entries_.push_back({key, path, &shown});
  }
  list_->set_visible(true);
  invalidate_layout();
  if (on_added_) {
    auto const handler = on_added_;
    auto const added = taken;
    handler(added);
  }
  return taken;
}

auto uploader::removed(std::uint64_t key) -> void {
  auto const it = std::ranges::find(entries_, key, &entry::key);
  if (it == entries_.end()) {
    return;
  }
  std::string const path = it->path;
  entries_.erase(it);
  list_->set_visible(!entries_.empty());
  invalidate_layout();
  if (on_removed_) {
    auto const handler = on_removed_;
    handler(path);
  }
}

auto uploader::clear() -> void {
  for (entry const &e : entries_) {
    if (tree() != nullptr) {
      (void)tree()->destroy_later(*e.shown);
    } else {
      (void)list_->remove_child(*e.shown);
    }
  }
  entries_.clear();
  list_->set_visible(false);
  invalidate_layout();
}

auto uploader::finish(std::string_view path, bool succeeded) -> void {
  for (entry const &e : entries_) {
    if (e.path == path) {
      e.shown->set_loading(false);
      e.shown->set_error(!succeeded);
    }
  }
}

auto uploader::set_error_message(std::string message) -> void {
  if (error_.set_text(std::move(message))) {
    invalidate_layout();
    invalidate_paint();
  }
}

auto uploader::select_files() -> bool {
  if (!enabled_in_tree() || tree() == nullptr || !tree()->file_dialog()) {
    return false;
  }
  file_dialog_request request;
  request.multiple = behavior_ != uploader_behavior::deferred_single;
  request.extensions = suffixes_;
  // The dialog may finish after this widget is gone.
  std::weak_ptr<uploader *> const alive = alive_;
  tree()->file_dialog().open(request, [alive](std::vector<std::string> paths) {
    if (auto const self = alive.lock(); self != nullptr && *self != nullptr) {
      (void)(*self)->add_files(std::move(paths));
    }
  });
  return true;
}

auto uploader::accessibility_info() const -> a11y::info {
  a11y::info details;
  details.kind = a11y::role::group;
  details.name = name_;
  details.description = error_.text();
  details.invalid = !error_.text().empty();
  return details;
}

auto uploader::on_event(event_context &, event const &e) -> bool {
  auto const *const drop = std::get_if<file_drop_event>(&e);
  if (drop == nullptr || layout_ == uploader_layout::no_drop_zone) {
    return false;
  }
  switch (drop->action) {
  case file_drop_action::enter:
  case file_drop_action::move:
    if (!dragging_) {
      dragging_ = true;
      invalidate_paint();
    }
    return true;
  case file_drop_action::leave:
    if (dragging_) {
      dragging_ = false;
      invalidate_paint();
    }
    return true;
  case file_drop_action::drop:
    dragging_ = false;
    invalidate_paint();
    (void)add_files(drop->paths);
    return true;
  }
  return false;
}

auto uploader::arrange(float width) const -> plan {
  basalt::theme const fallback;
  theme const &t = tree() != nullptr ? tree()->theme() : fallback;
  float const pad_x = resolve(t.sizes.inset_horizontal_drop_zone);
  float const pad_y = resolve(t.sizes.inset_vertical_drop_zone);
  float const gap = resolve(section_gap);
  plan p;
  p.button_size = select_->measure(constraints::loose({width, unbounded}));
  basalt::size const message = prompt_.measure(*this);
  float y = 0;
  float natural = 0;

  switch (layout_) {
  case uploader_layout::no_drop_zone:
    p.button = {0, 0};
    y = p.button_size.height;
    natural = p.button_size.width;
    break;
  case uploader_layout::compact: {
    float const inner = std::max(p.button_size.height, message.height);
    float const zone_width = std::max(width, 2 * pad_x + message.width + gap + p.button_size.width);
    p.zone = {{0, 0}, {zone_width, inner + 2 * pad_y}};
    p.prompt = {pad_x, pad_y + (inner - message.height) / 2};
    p.button = {zone_width - pad_x - p.button_size.width,
                pad_y + (inner - p.button_size.height) / 2};
    y = p.zone.height();
    natural = zone_width;
    break;
  }
  case uploader_layout::spacious: {
    float const inner = message.height + gap + p.button_size.height;
    float const zone_width =
        std::max(width, 2 * pad_x + std::max(message.width, p.button_size.width));
    p.zone = {{0, 0}, {zone_width, inner + 4 * pad_y}};
    p.prompt = {(zone_width - message.width) / 2, 2 * pad_y};
    p.button = {(zone_width - p.button_size.width) / 2, 2 * pad_y + message.height + gap};
    y = p.zone.height();
    natural = zone_width;
    break;
  }
  }

  float const content_width = std::max(width, natural);
  if (list_->visible()) {
    y += gap;
    p.list = {0, y};
    p.list_size = list_->measure(constraints::loose({content_width, unbounded}));
    y += p.list_size.height;
  }
  if (!error_.text().empty()) {
    y += gap;
    p.error = {0, y};
    y += error_.measure(*this).height;
  }
  p.size = {content_width, y};
  return p;
}

auto uploader::on_measure(constraints const &limits) const -> basalt::size {
  float const width = limits.bounded_width() ? limits.max.width : 0;
  return arrange(width).size;
}

auto uploader::on_layout() -> void {
  plan const p = arrange(bounds().width());
  select_->set_bounds({p.button, p.button_size});
  list_->set_bounds({p.list, {bounds().width(), p.list_size.height}});
}

auto uploader::on_paint(render_context &context) const -> void {
  if (tree() == nullptr) {
    return;
  }
  theme const &t = tree()->theme();
  plan const p = arrange(bounds().width());
  bool const failed = !error_.text().empty();
  if (layout_ != uploader_layout::no_drop_zone) {
    float const radius = resolve(t.sizes.radius_default);
    float const stroke = device_stroke_width(fixed(1), scale());
    context.fill_rounded_rect(p.zone, radius,
                              dragging_ ? t.colors.background_accent_transparent_hover
                                        : t.colors.background_default);
    rgba const border = failed      ? t.colors.border_danger
                        : dragging_ ? t.colors.border_accent
                                    : t.colors.border_default;
    context.stroke_rounded_rect(inset(p.zone, insets::uniform(stroke / 2)), radius, border, stroke);
    prompt_.draw(context, *this, p.prompt, t.colors.text_default);
  }
  if (failed) {
    error_.draw(context, *this, p.error, t.colors.text_danger);
  }
}

} // namespace basalt
