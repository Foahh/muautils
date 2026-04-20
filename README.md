# muautils

C++23 libraries and a small CLI (`mua`) for audio and image processing.

Audio is built on FFmpeg.
Image work uses libjpeg-turbo, libpng, libwebp, and [bc7enc_rdo](https://github.com/richgel999/bc7enc_rdo) for DDS output.

## Requirements

- CMake 3.28+
- C++23 toolchain
- [vcpkg](https://github.com/microsoft/vcpkg)
- Git

## Build

```bash
git clone --recurse-submodules <repository-url>

export VCPKG_ROOT=/path/to/vcpkg
cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build
```

On Windows, use the `vcpkg` preset in `CMakePresets.json` (targets `x64-windows-static`).

**Tests:**
```bash
ctest --test-dir build --output-on-failure
```

**Install:**
```bash
cmake --install build
```

Installs the `mua` binary, static libraries, public headers under `src/`, and a CMake package at `lib/cmake/muautils`.

## CLI

```bash
mua [--loglevel trace|debug|info|warn|error|critical|off] <subcommand> [options]
```

| Subcommand | Options |
|---|---|
| `audio_normalize` | `-s` `-d` `[-o offset]` |
| `audio_check` | `-s` |
| `image_check` | `-s` |
| `convert_jacket` | `-s` `-d` |
| `convert_stage` | `-b` `-s/--stsrc` `-d/--stdst` `[--fx1..--fx4]` |
| `extract_dds` | `-s` `-d` |

Exit codes: `0` success, `1` error, `2` no-op.

## Libraries

Link `mua_audio` or `mua_image` and include from `src/`:

- `audio/audio.hpp` — `Initialize()`, `EnsureValid(path)`, `Normalize(src, dst, offset)`
- `image/image.hpp` — `Initialize()`, `EnsureValid`, `ConvertJacket`, `ConvertStage`, `ExtractDds`

## License

LGPLv2.1. Third-party dependencies retain their own licenses.