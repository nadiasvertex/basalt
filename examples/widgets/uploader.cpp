// An uploader that accepts PNG and text files. The program adds two accepted files and one rejected
// file.

#include <print>

#include "example_host.h"

auto main(int argc, char **argv) -> int {
  auto host = example::example_host{{420, 200}};

  auto &files = host.content().emplace_child<basalt::uploader>("Attachments");
  files.set_prompt("Drop files here");
  files.set_accepted_suffixes({".png", ".txt"});
  files.set_on_added([](std::vector<std::string> const &paths) {
    std::println("{} file(s) were added.", paths.size());
  });
  host.content().set_cross_alignment(basalt::alignment::stretch);

  host.frame();
  files.add_files({"notes.txt", "photo.png", "program.exe"});
  host.frame();
  std::println("The uploader holds {} file(s).", files.files().size());
  std::println("The error message is \"{}\".", files.error_message());

  return host.finish(argc, argv);
}
