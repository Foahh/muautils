// src/image/detail/chunk.cpp
#include "chunk.hpp"

#include <algorithm>
#include <fmt/format.h>
#include <fstream>
#include <string_view>

namespace Image::detail {

namespace {

[[nodiscard]] std::vector<size_t> FindAllOccurrences(const std::span<const uint8_t> haystack,
                                                     const std::span<const uint8_t> needle) {
    std::vector<size_t> out;
    if (needle.empty() || haystack.size() < needle.size())
        return out;
    const std::string_view hay(reinterpret_cast<const char *>(haystack.data()), haystack.size());
    const std::string_view ndl(reinterpret_cast<const char *>(needle.data()), needle.size());
    size_t pos = 0;
    while ((pos = hay.find(ndl, pos)) != std::string_view::npos) {
        out.push_back(pos);
        pos += ndl.size();
    }
    return out;
}

void WriteBytes(std::ofstream &out, const fs::path &path, const std::span<const uint8_t> bytes) {
    if (bytes.empty())
        return;
    out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!out)
        throw lib::FileError(path, "Failed to write file");
}

} // namespace

std::vector<uint8_t> ReadFileData(const fs::path &path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw lib::FileError(path, "Failed to open file");
    }
    const std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char *>(buffer.data()), size)) {
        throw lib::FileError(path, "Failed to read file");
    }
    return buffer;
}

std::optional<size_t> FindChunks(const std::span<const uint8_t> haystack, const std::span<const uint8_t> needle,
                                 const size_t start) {
    if (start >= haystack.size() || needle.empty() || haystack.size() - start < needle.size()) {
        return std::nullopt;
    }
    const std::string_view hay(reinterpret_cast<const char *>(haystack.data()), haystack.size());
    const std::string_view ndl(reinterpret_cast<const char *>(needle.data()), needle.size());
    const size_t pos = hay.find(ndl, start);
    if (pos == std::string_view::npos) {
        return std::nullopt;
    }
    return pos;
}

std::vector<std::pair<size_t, size_t>> LocateChunks(const std::span<const uint8_t> data,
                                                    const std::span<const uint8_t> header,
                                                    const std::span<const uint8_t> footer) {
    const auto headers = FindAllOccurrences(data, header);
    const auto footers = FindAllOccurrences(data, footer);

    std::vector<std::pair<size_t, size_t>> chunks;
    chunks.reserve(headers.size());

    auto footerIt = footers.begin();
    for (size_t i = 0; i < headers.size(); ++i) {
        const size_t start = headers[i];
        const size_t searchAfter = start + header.size();
        const size_t nextHeader = (i + 1 < headers.size()) ? headers[i + 1] : data.size();

        // First footer at or after searchAfter.
        footerIt = std::lower_bound(footerIt, footers.end(), searchAfter);

        size_t end = data.size();
        const bool foundStop = footerIt != footers.end();
        const bool foundNextHeader = (i + 1 < headers.size());
        if (foundStop && foundNextHeader) {
            end = (std::min)(*footerIt, nextHeader);
        } else if (foundStop) {
            end = *footerIt;
        } else if (foundNextHeader) {
            end = nextHeader;
        }
        chunks.emplace_back(start, end);
        if (!foundStop && !foundNextHeader)
            break;
    }
    return chunks;
}

std::vector<std::pair<size_t, size_t>> LocateDdsChunks(const std::span<const uint8_t> data) {
    constexpr uint8_t ddsHeader[] = {'D', 'D', 'S', ' '};
    constexpr uint8_t ddsStopSign[] = {'P', 'O', 'F', '0'};
    return LocateChunks(data, ddsHeader, ddsStopSign);
}

void ExtractChunks(const std::span<const uint8_t> data, const fs::path &dstFolder, const fs::path &baseName,
                   const fs::path &extension, const std::vector<std::pair<size_t, size_t>> &chunks) {
    fs::create_directories(dstFolder);
    for (size_t i = 0; i < chunks.size(); ++i) {
        auto [start, end] = chunks[i];
        if (start > end || end > data.size()) {
            throw std::out_of_range(
                fmt::format("Invalid chunk range: [{}, {}) for data size {}", start, end, data.size()));
        }
        auto path = dstFolder / baseName;
        path += fmt::format("_{:04d}", i + 1);
        path += extension;
        std::ofstream file(path, std::ios::binary);
        if (!file)
            throw lib::FileError(path, "Failed to create file");
        file.write(reinterpret_cast<const char *>(data.data() + start), static_cast<std::streamsize>(end - start));
        if (!file)
            throw lib::FileError(path, "Failed to write file");
    }
}

void ReplaceChunks(const std::span<const uint8_t> data, const fs::path &dstPath,
                   const std::vector<std::pair<size_t, size_t>> &chunks,
                   const std::vector<std::optional<std::span<const uint8_t>>> &replacements) {
    if (replacements.size() < chunks.size()) {
        throw std::out_of_range(
            fmt::format("Replacements size {} < chunks size {}", replacements.size(), chunks.size()));
    }

    size_t cursor = 0;
    for (const auto [s, e] : chunks) {
        if (s < cursor || s > e || e > data.size()) {
            throw std::out_of_range(
                fmt::format("Invalid chunk range: [{}, {}) after cursor {} for data size {}", s, e, cursor,
                            data.size()));
        }
        cursor = e;
    }

    std::ofstream outFile(dstPath, std::ios::binary);
    if (!outFile)
        throw lib::FileError(dstPath, "Failed to create file");

    cursor = 0;
    for (size_t i = 0; i < chunks.size(); ++i) {
        auto [s, e] = chunks[i];
        WriteBytes(outFile, dstPath, data.subspan(cursor, s - cursor));
        if (replacements[i].has_value()) {
            const auto &repl = replacements[i].value();
            WriteBytes(outFile, dstPath, repl);
        } else {
            WriteBytes(outFile, dstPath, data.subspan(s, e - s));
        }
        cursor = e;
    }
    WriteBytes(outFile, dstPath, data.subspan(cursor));
}

} // namespace Image::detail
