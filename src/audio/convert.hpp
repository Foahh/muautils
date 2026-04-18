#pragma once
#include "audio/audio.hpp"

namespace Audio {
bool Normalize(const fs::path &srcPath, const fs::path &dstPath, double offset = 0.0);
}
