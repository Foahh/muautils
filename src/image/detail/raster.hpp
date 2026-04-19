#pragma once

#include "lib.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace Image::detail {

struct RgbaImage {
    unsigned width = 0;
    unsigned height = 0;
    std::vector<uint8_t> pixels;

    [[nodiscard]] std::span<const uint8_t> span() const noexcept {
        return pixels;
    }
};

void ValidateImage(const fs::path &path);

[[nodiscard]] RgbaImage LoadRgba(const fs::path &path);

[[nodiscard]] RgbaImage LoadResizedRgba(const fs::path &path, int width, int height);

[[nodiscard]] RgbaImage MakeBlankRgba(unsigned width, unsigned height);

[[nodiscard]] RgbaImage JoinTiles2x2(const std::array<RgbaImage, 4> &tiles);

} // namespace Image::detail
