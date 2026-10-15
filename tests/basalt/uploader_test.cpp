#include <catch2/catch_test_macros.hpp>

#include "basalt/widgets/box.h"
#include "basalt/widgets/uploader.h"

#include <memory>
#include <string>
#include <vector>

#include "ui_fixture.h"

using namespace basalt;
using namespace basalt::testing;
namespace a11y = basalt::accessibility;

namespace {

struct fixture {
  explicit fixture(uploader_layout layout = uploader_layout::spacious,
                   uploader_behavior behavior = uploader_behavior::deferred_multiple) {
    root = &f.tree.emplace_root<box>(axis::vertical);
    root->set_cross_alignment(alignment::start);
    drop_zone = &root->emplace_child<uploader>("Attachments", layout, behavior);
    drop_zone->set_on_added([this](std::vector<std::string> const &paths) {
      for (auto const &p : paths) {
        added.push_back(p);
      }
    });
    drop_zone->set_on_removed([this](std::string const &path) { removed.push_back(path); });
    f.tree.set_file_dialog(
        {[this](file_dialog_request const &request, file_dialog::completion done) {
          last_request = request;
          pending = std::move(done);
        }});
    f.layout();
  }

  auto drop(std::vector<std::string> paths) -> bool {
    return f.send(file_drop_event{.action = file_drop_action::drop,
                                  .position = drop_zone->bounds().center(),
                                  .paths = std::move(paths)});
  }
  auto drag(file_drop_action action) -> bool {
    return f.send(file_drop_event{.action = action, .position = drop_zone->bounds().center()});
  }

  ui f;
  box *root = nullptr;
  uploader *drop_zone = nullptr;
  std::vector<std::string> added;
  std::vector<std::string> removed;
  file_dialog_request last_request;
  file_dialog::completion pending;
};

} // namespace

TEST_CASE("dropped files are listed as chips", "[uploader]") {
  fixture f;
  REQUIRE(f.drop({"/docs/report.pdf", "/docs/photo.png"}));
  REQUIRE(f.drop_zone->files() == std::vector<std::string>{"/docs/report.pdf", "/docs/photo.png"});
  REQUIRE(f.added.size() == 2);
}

TEST_CASE("dragging over the zone shows the drag state", "[uploader]") {
  fixture f;
  REQUIRE(f.drag(file_drop_action::enter));
  REQUIRE(f.drop_zone->dragging());
  REQUIRE(f.drag(file_drop_action::leave));
  REQUIRE_FALSE(f.drop_zone->dragging());
  f.drag(file_drop_action::move);
  f.drop({"/a.txt"});
  REQUIRE_FALSE(f.drop_zone->dragging());
}

TEST_CASE("the layout without a drop zone ignores drops", "[uploader]") {
  fixture f{uploader_layout::no_drop_zone};
  REQUIRE_FALSE(f.drop({"/a.txt"}));
  REQUIRE(f.drop_zone->files().empty());
}

TEST_CASE("Select Files asks the application's file dialog", "[uploader]") {
  fixture f;
  REQUIRE(f.drop_zone->select_files());
  REQUIRE(f.last_request.multiple);
  f.pending({"/docs/a.txt"});
  REQUIRE(f.drop_zone->files() == std::vector<std::string>{"/docs/a.txt"});
}

TEST_CASE("without a file dialog the button does nothing", "[uploader]") {
  fixture f;
  f.f.tree.set_file_dialog({});
  REQUIRE_FALSE(f.drop_zone->select_files());
}

TEST_CASE("a dialog that finishes after the uploader is gone is ignored", "[uploader]") {
  fixture f;
  f.drop_zone->select_files();
  f.f.tree.destroy_later(*f.drop_zone);
  f.f.tree.collect_garbage();
  REQUIRE_NOTHROW(f.pending({"/late.txt"}));
  REQUIRE(f.added.empty());
}

TEST_CASE("unaccepted files set the error message", "[uploader]") {
  fixture f;
  f.drop_zone->set_accepted_suffixes({".PNG", ".jpg"});
  f.drop({"/docs/notes.txt", "/docs/Photo.png"});
  REQUIRE(f.drop_zone->files() == std::vector<std::string>{"/docs/Photo.png"});
  REQUIRE(f.drop_zone->error_message() == "notes.txt is not an accepted file type.");

  f.drop({"/docs/other.JPG"});
  REQUIRE(f.drop_zone->error_message().empty());
}

TEST_CASE("the single behavior keeps only the latest file", "[uploader]") {
  fixture f{uploader_layout::compact, uploader_behavior::deferred_single};
  f.drop({"/a.txt", "/b.txt"});
  REQUIRE(f.drop_zone->files() == std::vector<std::string>{"/a.txt"});
  f.drop({"/c.txt"});
  REQUIRE(f.drop_zone->files() == std::vector<std::string>{"/c.txt"});
  f.f.tree.collect_garbage();
  f.f.layout();
}

TEST_CASE("removing a chip removes the file and reports it", "[uploader]") {
  fixture f;
  f.drop({"/a.txt", "/b.txt"});
  f.f.layout();
  chip_group *list = nullptr;
  for (widget &c : f.drop_zone->children()) {
    if (auto *g = dynamic_cast<chip_group *>(&c)) {
      list = g;
    }
  }
  REQUIRE(list != nullptr);
  REQUIRE(list->chip_at(0).remove());
  REQUIRE(f.removed == std::vector<std::string>{"/a.txt"});
  REQUIRE(f.drop_zone->files() == std::vector<std::string>{"/b.txt"});
}

TEST_CASE("the immediate behavior marks chips busy until they finish", "[uploader]") {
  fixture f{uploader_layout::spacious, uploader_behavior::immediate};
  f.drop({"/a.txt", "/b.txt"});
  chip_group *list = nullptr;
  for (widget &c : f.drop_zone->children()) {
    if (auto *g = dynamic_cast<chip_group *>(&c)) {
      list = g;
    }
  }
  REQUIRE(list != nullptr);
  REQUIRE(list->chip_count() == 2);
  REQUIRE(list->chip_at(0).loading());
  f.drop_zone->finish("/a.txt", true);
  f.drop_zone->finish("/b.txt", false);
  REQUIRE_FALSE(list->chip_at(0).loading());
  REQUIRE_FALSE(list->chip_at(0).has_error());
  REQUIRE(list->chip_at(1).has_error());
}

TEST_CASE("the spacious zone is taller than the compact one", "[uploader]") {
  fixture spacious{uploader_layout::spacious};
  fixture compact{uploader_layout::compact};
  REQUIRE(spacious.drop_zone->bounds().height() > compact.drop_zone->bounds().height());
}

TEST_CASE("the uploader reports its error to assistive technology", "[uploader][accessibility]") {
  fixture f;
  f.drop_zone->set_error_message("Too large");
  auto snapshot = f.f.semantics();
  auto const *node = snapshot.find(f.drop_zone->id());
  REQUIRE(node->semantics.name == "Attachments");
  REQUIRE(node->semantics.invalid);
  REQUIRE(node->semantics.description == "Too large");
  REQUIRE(a11y::audit(snapshot).empty());
}
