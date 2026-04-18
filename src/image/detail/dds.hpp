// src/image/detail/dds.hpp
#pragma once

#include "lib.hpp"

#include <array>
#include <vector>

#include <DirectXTex.h>

namespace Image::detail {

DirectX::Blob ConvertBackground(const fs::path &srcPath);

DirectX::Blob ConvertEffect(const std::array<fs::path, 4> &srcPaths);

void SaveJacketDds(std::vector<uint8_t> rgba, unsigned width, unsigned height,
                   const fs::path &dstPath);

} // namespace Image::detail
