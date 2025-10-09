#pragma once
#include "audio.hpp"

namespace Audio {
bool Normalize(const fs::path &srcPath, const fs::path &dstPath, double offset = 0.0,
               const NormalizeFormat &target = FMT_PCM_S16LE_8LU);
}

