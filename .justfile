# Justfile for basalt

# List available recipes
default:
    @just --list

[doc('Format all C++ sources with clang-format.')]
[group('format')]
format:
    git ls-files -co --exclude-standard -- 'include/*.h' 'src/*.cpp' 'src/*.h' 'tests/*.cpp' 'tests/*.h' 'tools/*.cpp' | xargs clang-format -i

[doc('Fail if any C++ source is not formatted.')]
[group('format')]
format-check:
    git ls-files -co --exclude-standard -- 'include/*.h' 'src/*.cpp' 'src/*.h' 'tests/*.cpp' 'tests/*.h' 'tools/*.cpp' | xargs clang-format --dry-run --Werror

[doc('Configure and build with a preset: debug, release or sanitize.')]
[group('build')]
build preset="debug":
    cmake --preset {{ preset }}
    cmake --build --preset {{ preset }}

[doc('Build and run all tests.')]
[group('test')]
test preset="debug": (build preset)
    ctest --preset {{ preset }}

[doc('Remove build trees.')]
[group('build')]
clean:
    rm -rf .build

[doc('Download and unpack the pinned dependency sources for offline builds.')]
[group('build')]
sources dir=".build/sources":
    cmake -D BASALT_SOURCES_DIR={{ dir }} -P cmake/fetch_sources.cmake

[doc('Install a release build into the given prefix.')]
[group('build')]
install prefix=".build/prefix": (build "release")
    cmake --install .build/release --prefix {{ prefix }}

[doc('Build tests/consumer against an installed Basalt, as another project would.')]
[group('test')]
consumer: (install ".build/prefix")
    cmake -S tests/consumer -B .build/consumer -G Ninja -DCMAKE_BUILD_TYPE=Release -DBASALT_CONSUME=package -DCMAKE_PREFIX_PATH="$PWD/.build/prefix"
    cmake --build .build/consumer
    ctest --test-dir .build/consumer --output-on-failure

[doc('Build and run the SDL and Skia example. The first build compiles Skia.')]
[group('build')]
demo-sdl-skia:
    cmake -S examples/sdl_skia -B .build/example -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build .build/example
    .build/example/basalt_sdl_skia

[doc('Build and run the damage-rect demo on the GPU, falling back to the CPU. Compiles a second Skia.')]
[group('build')]
demo-sdl-skia-gpu *args:
    cmake -S examples/sdl_skia_gpu -B .build/example-gpu -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build .build/example-gpu
    .build/example-gpu/basalt_sdl_skia_gpu {{ args }}

[doc('Regenerate generated/basalt/tokens.h from the Cubit DTCG tokens.')]
[group('build')]
tokens tokens_dir="../cubit/packages/cubit-styles/src/tokens":
    cmake --preset debug
    cmake --build --preset debug --target basalt_tokens
    .build/debug/tools/basalt_tokens/basalt_tokens {{ tokens_dir }} generated/basalt/tokens.h

[doc('Raise the CMakeLists.txt project version by "major", "minor" or "patch".')]
[group('release')]
bump-version part:
    #!/usr/bin/env bash
    set -euo pipefail
    case "{{ part }}" in
      major|minor|patch) ;;
      *) echo "part must be major, minor, or patch (got '{{ part }}')" >&2; exit 1 ;;
    esac
    current=$(sed -nE 's/^project\(basalt VERSION ([0-9]+\.[0-9]+\.[0-9]+).*/\1/p' CMakeLists.txt)
    if [ -z "$current" ]; then
      echo "CMakeLists.txt has no 'project(basalt VERSION X.Y.Z' line" >&2
      exit 1
    fi
    IFS=. read -r major minor patch <<< "$current"
    case "{{ part }}" in
      major) major=$((major + 1)); minor=0; patch=0 ;;
      minor) minor=$((minor + 1)); patch=0 ;;
      patch) patch=$((patch + 1)) ;;
    esac
    next="$major.$minor.$patch"
    sed -E -i.bak "s/^(project\(basalt VERSION )[0-9.]+/\1$next/" CMakeLists.txt
    rm CMakeLists.txt.bak
    echo "$current -> $next"

[doc('Tag HEAD as v<version> from CMakeLists.txt and push the tag, which starts the GitHub release build.')]
[group('release')]
release:
    #!/usr/bin/env bash
    set -euo pipefail
    version=$(sed -nE 's/^project\(basalt VERSION ([0-9]+\.[0-9]+\.[0-9]+).*/\1/p' CMakeLists.txt)
    if [ -z "$version" ]; then
      echo "CMakeLists.txt has no 'project(basalt VERSION X.Y.Z' line" >&2
      exit 1
    fi
    tag="v$version"
    if git rev-parse -q --verify "refs/tags/$tag" >/dev/null \
       || git ls-remote --exit-code --tags origin "refs/tags/$tag" >/dev/null 2>&1; then
      echo "Tag $tag already exists. Run 'just bump-version <major|minor|patch>' and commit the change first." >&2
      exit 1
    fi
    if ! git diff --quiet HEAD -- CMakeLists.txt; then
      echo "CMakeLists.txt has uncommitted changes. Commit the version bump before releasing." >&2
      exit 1
    fi
    git tag -a "$tag" -m "Basalt $tag"
    git push origin "$tag"
    echo "Pushed $tag"
