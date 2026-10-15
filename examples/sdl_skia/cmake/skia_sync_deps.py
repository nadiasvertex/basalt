"""Sync only the Skia third-party checkouts the example build uses.

Skia's tools/git-sync-deps fetches every entry in DEPS (dozens of repositories,
most unused here). This writes a filtered DEPS file with absolute checkout
paths, optionally rewrites URLs to a mirror, and reuses Skia's own sync code so
commits stay exactly as pinned by the Skia revision.

Usage: skia_sync_deps.py <skia-src> <filtered-deps-out> [--mirror=<url>] [<name>...]
"""

import importlib.machinery
import importlib.util
import os
import sys


def load_git_sync_deps(skia_src):
    path = os.path.join(skia_src, "tools", "git-sync-deps")
    loader = importlib.machinery.SourceFileLoader("skia_git_sync_deps", path)
    spec = importlib.util.spec_from_loader(loader.name, loader)
    module = importlib.util.module_from_spec(spec)
    loader.exec_module(module)
    return module


def main(argv):
    if len(argv) < 2:
        sys.stderr.write(__doc__)
        return 2
    skia_src, out_path, names = argv[0], argv[1], argv[2:]
    mirror = ""
    if names and names[0].startswith("--mirror="):
        mirror, names = names[0][len("--mirror="):], names[1:]
    sync = load_git_sync_deps(skia_src)
    deps = sync.parse_file_to_dict(os.path.join(skia_src, "DEPS"))["deps"]

    selected = {}
    for name in names:
        key = "third_party/externals/" + name
        if key not in deps or not isinstance(deps[key], str):
            sys.stderr.write("Skia DEPS has no entry for %s\n" % key)
            return 1
        url, commit = deps[key].split("@", 1)
        if mirror:
            url = mirror.rstrip("/") + "/" + url.rstrip("/").rsplit("/", 1)[-1]
        selected[os.path.join(skia_src, key)] = url + "@" + commit

    with open(out_path, "w") as f:
        f.write("deps = %r\n" % selected)
    verbose = not os.environ.get("GIT_SYNC_DEPS_QUIET")
    sync.git_sync_deps(out_path, [], True, verbose)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
