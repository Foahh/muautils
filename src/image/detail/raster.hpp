#pragma once

#include "lib.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace Image::detail {

struct RgbaPixel {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;

    constexpr RgbaPixel() = default;
    constexpr RgbaPixel(const uint8_t cr, const uint8_t cg, const uint8_t cb, const uint8_t ca)
        : r(cr), g(cg), b(cb), a(ca) {}
    constexpr RgbaPixel(const uint8_t cy, const uint8_t ca = 255) : r(cy), g(cy), b(cy), a(ca) {}

    constexpr RgbaPixel &set(const uint8_t cy, const uint8_t ca = 255) noexcept {
        r = cy;
        g = cy;
        b = cy;
        a = ca;
        return *this;
    }

    constexpr RgbaPixel &set(const uint8_t cr, const uint8_t cg, const uint8_t cb, const uint8_t ca) noexcept {
        r = cr;
        g = cg;
        b = cb;
        a = ca;
        return *this;
    }
};

struct RgbaImage {
    unsigned width = 0;
    unsigned height = 0;
    std::vector<RgbaPixel> pixels;
};

void ValidateImage(const fs::path &path);

[[nodiscard]] RgbaImage LoadResizedRgba(const fs::path &path, int width, int height);

[[nodiscard]] RgbaImage MakeBlankRgba(unsigned width, unsigned height);

[[nodiscard]] RgbaImage JoinTiles2x2(const std::array<RgbaImage, 4> &tiles);

} // namespace Image::detail
