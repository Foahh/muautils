// src/image/detail/dds.hpp
#pragma once

#include "lib.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace Image::detail {

std::vector<uint8_t> ConvertBackground(const fs::path &srcPath);

std::vector<uint8_t> ConvertEffect(const std::array<fs::path, 4> &srcPaths);

void SaveJacketDds(std::vector<uint8_t> rgba, unsigned width, unsigned height,
                   const fs::path &dstPath);

} // namespace Image::detail
