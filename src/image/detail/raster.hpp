#pragma once

#include "lib.hpp"

#include <utils.h>

#include <array>
#include <cstdint>
#include <vector>

namespace Image::detail {

struct RgbaImage {
    unsigned width = 0;
    unsigned height = 0;
    std::vector<utils::color_quad_u8> pixels;
};

void ValidateImage(const fs::path &path);

[[nodiscard]] RgbaImage LoadResizedRgba(const fs::path &path, int width, int height);

[[nodiscard]] RgbaImage MakeBlankRgba(unsigned width, unsigned height);

[[nodiscard]] RgbaImage JoinTiles2x2(const std::array<RgbaImage, 4> &tiles);

} // namespace Image::detail
