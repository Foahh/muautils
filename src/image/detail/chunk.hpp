// src/image/detail/chunk.hpp
#pragma once

#include "lib.hpp"

#include <optional>
#include <span>
#include <vector>

namespace Image::detail {

std::vector<uint8_t> ReadFileData(const fs::path &path);

std::optional<size_t> FindChunks(std::span<const uint8_t> haystack, std::span<const uint8_t> needle, size_t start);

std::vector<std::pair<size_t, size_t>> LocateChunks(std::span<const uint8_t> data, std::span<const uint8_t> header,
                                                    std::span<const uint8_t> footer);

std::vector<std::pair<size_t, size_t>> LocateDdsChunks(std::span<const uint8_t> data);

void ExtractChunks(std::span<const uint8_t> data, const fs::path &dstFolder, const fs::path &baseName,
                   const fs::path &extension, const std::vector<std::pair<size_t, size_t>> &chunks);

void ReplaceChunks(std::span<const uint8_t> data, const fs::path &dstPath,
                   const std::vector<std::pair<size_t, size_t>> &chunks,
                   const std::vector<std::optional<std::span<const uint8_t>>> &replacements);

} // namespace Image::detail
