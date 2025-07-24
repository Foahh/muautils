#pragma once

#include "lib.hpp"

namespace Image {

void Initialize();

void EnsureValid(const fs::path &srcPath);

void ConvertJacket(const fs::path &srcPath, const fs::path &dstPath);

void ConvertStage(const fs::path &bgSrcPath, const fs::path &stSrcPath,
                  const fs::path &stDstPath,
                  const std::array<fs::path, 4> &fxSrcPaths);

void ExtractDds(const fs::path &srcPath, const fs::path &dstFolder);

} // namespace Image