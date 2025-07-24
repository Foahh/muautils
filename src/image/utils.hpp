#pragma once

#include "lib.hpp"
#include "utils.hpp"

#include <filesystem>
#include <span>
#include <fmt/format.h>
#include <fstream>
#include <optional>
#include <vector>

#include <FreeImagePlus.h>

namespace Image {

inline std::optional<size_t> FindChunks(const std::span<const uint8_t> haystack, const std::span<const uint8_t> needle,
                                        const size_t start) {
    if (start >= haystack.size() || needle.empty() || haystack.size() - start < needle.size()) {
        return std::nullopt;
    }

    const std::string_view hay(reinterpret_cast<std::string_view::const_pointer>(haystack.data()), haystack.size());
    const std::string_view ndl(reinterpret_cast<std::string_view::const_pointer>(needle.data()), needle.size());
    size_t pos = hay.find(ndl, start);
    if (pos == std::string_view::npos) {
        return std::nullopt;
    }
    return pos;
}

inline std::vector<std::pair<size_t, size_t> > LocateChunks(const std::span<const uint8_t> data,
                                                            const std::span<const uint8_t> header,
                                                            const std::span<const uint8_t> footer) {
    std::vector<std::pair<size_t, size_t> > chunks;
    size_t currentPos = 0;

    while (true) {
        auto startOpt = FindChunks(data, header, currentPos);
        if (!startOpt) {
            break;
        }
        size_t start = *startOpt;

        const size_t searchAfter = start + header.size();
        auto stopPos = FindChunks(data, footer, searchAfter);
        auto nextHeader = FindChunks(data, header, searchAfter);

        size_t end = data.size();
        const bool foundStop = stopPos.has_value();
        const bool foundNextHeader = nextHeader.has_value();

        if (foundStop && foundNextHeader) {
            end = (std::min)(*stopPos, *nextHeader);
        } else if (foundStop) {
            end = *stopPos;
        } else if (foundNextHeader) {
            end = *nextHeader;
        }

        chunks.emplace_back(start, end);

        if (!foundStop && !foundNextHeader) {
            break;
        }
        currentPos = end;
    }
    return chunks;
}

inline void ExtractChunks(const std::span<const uint8_t> data, const fs::path &dstFolder, const fs::path &baseName,
                          const fs::path &extension, const std::vector<std::pair<size_t, size_t> > &chunks) {
    fs::create_directories(dstFolder);

    for (size_t i = 0; i < chunks.size(); ++i) {
        auto [start, end] = chunks[i];

        if (start > end || end > data.size()) {
            const auto msg = fmt::format("Invalid chunk range: [{}, {}) for data size {}", start, end, data.size());
            throw std::out_of_range(msg);
        }

        auto path = dstFolder / baseName;
        path += fmt::format("_{:04d}", i + 1);
        path += extension;

        std::ofstream file(path, std::ios::binary);
        if (!file) {
            throw lib::FileError(path, "Failed to create file");
        }

        file.write(reinterpret_cast<const char *>(data.data() + start), static_cast<std::streamsize>(end - start));
        if (!file) {
            throw lib::FileError(path, "Failed to write file");
        }
    }
}

inline void ReplaceChunks(const std::span<const uint8_t> data, const fs::path &dstPath,
                          const std::vector<std::pair<size_t, size_t> > &chunks,
                          const std::vector<std::optional<std::span<const uint8_t> > > &replacements) {
    if (replacements.size() < chunks.size()) {
        const auto msg = fmt::format("Replacements size {} < chunks size {}", replacements.size(), chunks.size());
        throw std::out_of_range(msg);
    }

    std::ofstream outFile(dstPath, std::ios::binary);
    if (!outFile) {
        throw lib::FileError(dstPath, "Failed to create file");
    }

    size_t cursor = 0;
    for (size_t i = 0; i < chunks.size(); ++i) {
        auto [s, e] = chunks[i];
        outFile.write(reinterpret_cast<const char *>(data.data() + cursor), static_cast<std::streamsize>(s - cursor));

        if (replacements[i].has_value()) {
            auto &repl = replacements[i].value();
            outFile.write(reinterpret_cast<const char *>(repl.data()), static_cast<std::streamsize>(repl.size()));
        } else {
            outFile.write(reinterpret_cast<const char *>(data.data() + s), static_cast<std::streamsize>(e - s));
        }
        cursor = e;
    }

    outFile.write(reinterpret_cast<const char *>(data.data() + cursor),
                  static_cast<std::streamsize>(data.size() - cursor));
}

inline std::vector<uint8_t> ReadFileData(const fs::path &path) {
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

inline std::vector<std::pair<size_t, size_t> > LocateDdsChunks(const std::span<const uint8_t> data) {
    constexpr uint8_t ddsHeader[] = {'D', 'D', 'S', ' '};
    constexpr uint8_t ddsStopSign[] = {'P', 'O', 'F', '0'};
    return LocateChunks(data, ddsHeader, ddsStopSign);
}

inline FREE_IMAGE_FORMAT GetFreeImageFormat(const fs::path &path) {
#ifdef _WIN32
    const auto fif = FreeImage_GetFileTypeU(path.c_str());
    return fif != FIF_UNKNOWN ? fif : FreeImage_GetFIFFromFilenameU(path.c_str());
#else
    const auto fif = FreeImage_GetFileType(path.c_str());
    return fif != FIF_UNKNOWN ? fif : FreeImage_GetFIFFromFilename(path.c_str());
#endif
}

inline bool LoadFipImage(fipImage &img, const fs::path &path) {
#ifdef _WIN32
    return img.loadU(path.c_str());
#else
    return img.load(path.c_str());
#endif
}

inline fipImage LoadFip32Image(const fs::path &srcPath) {
    fipImage img;
    if (!LoadFipImage(img, srcPath)) {
        throw lib::FileError(srcPath, "Failed to load image");
    }
    if (img.getImageType() != FIT_BITMAP || img.getBitsPerPixel() != 32) {
        if (!img.convertTo32Bits()) {
            throw lib::FileError(srcPath, "Failed to convert image to 32bpp");
        }
    }
    return img;
}

inline bool IsImageValid(const fs::path &srcPath) {
    const auto fif = GetFreeImageFormat(srcPath);
    return fif != FIF_UNKNOWN && FreeImage_FIFSupportsReading(fif);
}

} // namespace Image