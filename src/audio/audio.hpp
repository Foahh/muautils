#pragma once

#include "lib.hpp"

namespace Audio {

void Initialize();

void EnsureValid(const fs::path &path);

bool Normalize(const fs::path &src, const fs::path &dst, double offset = 0.0);

} // namespace Audio
