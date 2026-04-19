# muautils

C++23 utilities for audio and image processing: static libraries you can link into your own tools, plus a small CLI named `mua`.

Audio work is built on **FFmpeg**. Image work uses **libjpeg-turbo**, **libpng**, and **libwebp** for decoding, **bc7enc_rdo** (vendored) for BC7 DDS output, and related helpers for jacket/stage conversion and DDS extraction.

## Requirements

- **CMake** 3.28 or newer
- A C++23 toolchain
- [**vcpkg**](https://github.com/microsoft/vcpkg) (manifest mode is supported; this repo pins a baseline in `vcpkg-configuration.json` and adds an **overlay port** for FFmpeg under `ports/`)
- Image codec dependencies resolved through **vcpkg** (`libjpeg-turbo`, `libpng`, and `libwebp`)
- **OpenMP** for C++: **MSVC** uses the built-in runtime (`/openmp` + `vcomp` / `vcompd`); other toolchains need CMake’s `FindOpenMP` (e.g. GCC/Clang on Linux with `libgomp`, Apple Clang often with [Homebrew `libomp`](https://formulae.brew.sh/formula/libomp)). The installed CMake package still calls `find_dependency(OpenMP)` so consumers link OpenMP correctly when they use the static libraries.
- **Git** (for submodules)

## Clone and submodules

The image pipeline embeds [bc7enc_rdo](https://github.com/richgel999/bc7enc_rdo) as a submodule:

```bash
git clone --recurse-submodules <repository-url>
# or, if you already cloned:
git submodule update --init --recursive
```

If the submodule is missing, CMake fails with a message pointing at this step.

## Configure and build

Point CMake at the vcpkg toolchain so manifest dependencies resolve (CLI11, Catch2, spdlog, fmt, libjpeg-turbo, libpng, libwebp, and the custom FFmpeg feature from `vcpkg.json`):

```bash
export VCPKG_ROOT=/path/to/vcpkg
cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build
```

On Windows, the repo includes a **CMake preset** (`vcpkg`) in `CMakePresets.json` that sets `VCPKG_TARGET_TRIPLET` to `x64-windows-static` and the toolchain file from `VCPKG_ROOT`.

### Tests

```bash
ctest --test-dir build --output-on-failure
```

Or run `test_audio` and `test_image` from the build directory.

### Install

```bash
cmake --install build
```

This installs the `mua` binary, static libraries `mua_audio` and `mua_image`, the interface target headers under `src/` (public `.hpp` files only), and a CMake package (`muautilsConfig.cmake`) under the usual `lib/cmake/muautils` layout.

## Command-line tool (`mua`)

Global option:

- `--loglevel` — `trace`, `debug`, `info`, `warn`, `error`, `critical`, or `off` (default: `info`)

Subcommands:

| Subcommand | Purpose |
|------------|---------|
| `audio_normalize` | Normalize audio (`-s` / `--src`, `-d` / `--dst`, optional `-o` / `--offset` seconds) |
| `audio_check` | Validate an audio file (`-s` / `--src`) |
| `image_check` | Validate an image (`-s` / `--src`) |
| `convert_jacket` | Convert jacket art (`-s`, `-d`) |
| `convert_stage` | Convert stage assets: background (`-b`), source (`-s` / `--stsrc`), destination (`-d` / `--stdst`), optional effect layers `--fx1` … `--fx4` |
| `extract_dds` | Extract DDS data from a source file into a folder (`-s`, `-d`) |

Example:

```bash
./build/mua audio_normalize -s in.wav -d out.wav
./build/mua image_check -s cover.png
```

Exit codes: success `0`, error `1`, no-op `2` (used where applicable, e.g. some audio normalize paths).

## Libraries (for embedding)

Headers live under `src/`; installed layouts mirror that for public `.hpp` files (detail namespaces may be present in-tree).

- **`Audio`** (`audio/audio.hpp`): `Initialize()`, `EnsureValid(path)`, `Normalize(src, dst, offset)` → `bool`
- **`Image`** (`image/image.hpp`): `Initialize()`, `EnsureValid`, `ConvertJacket`, `ConvertStage`, `ExtractDds`

Link `mua_audio` / `mua_image` and satisfy their transitive dependencies (FFmpeg for audio, libjpeg-turbo/libpng/libwebp and the bc7 object library for image, plus spdlog/fmt via the common interface).

## License

This project is licensed under the **GNU Lesser General Public License v2.1** — see [LICENSE](LICENSE). Third-party code (vcpkg ports, FFmpeg, libjpeg-turbo, libpng, libwebp, bc7enc_rdo, etc.) remains under their respective licenses.
