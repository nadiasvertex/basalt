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

[doc('Regenerate generated/basalt/tokens.h from the Cubit DTCG tokens.')]
[group('build')]
tokens tokens_dir="../cubit/packages/cubit-styles/src/tokens":
    cmake --preset debug
    cmake --build --preset debug --target basalt_tokens
    .build/debug/tools/basalt_tokens/basalt_tokens {{ tokens_dir }} generated/basalt/tokens.h
