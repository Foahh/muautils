// src/image/detail/vips.hpp
#pragma once

#include "lib.hpp"

#include <string>
#include <vector>

#include <vips/vips8>

namespace Image::detail {

std::string PathToVipsUtf8(const fs::path &path);

vips::VImage LoadVipsImage(const fs::path &path);

vips::VImage ToRgbaUchar(vips::VImage img);

vips::VImage LanczosResizeTo(vips::VImage img, int w, int h);

std::vector<uint8_t> RgbaPixelsFrom(const vips::VImage &img);

} // namespace Image::detail
