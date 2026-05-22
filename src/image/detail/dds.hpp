// src/image/detail/dds.hpp
#pragma once

#include "lib.hpp"
#include "raster.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace Image::detail {

enum class DdsCompression {
    Bc1,
    Bc3
};

[[nodiscard]] std::vector<uint8_t> EncodeDds(std::span<const uint8_t> rgba, unsigned width, unsigned height,
                                             DdsCompression compression);

[[nodiscard]] std::vector<uint8_t> EncodeDds(RgbaImage image, DdsCompression compression);

void SaveDds(const fs::path &dstPath, std::span<const uint8_t> bytes);

} // namespace Image::detail
