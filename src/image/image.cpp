// src/image/image.cpp
#include "image.hpp"

#include <future>

#include "detail/chunk.hpp"
#include "detail/dds.hpp"
#include "detail/raster.hpp"

using namespace Image::detail;

void Image::Initialize() {
    // no-op
}

void Image::EnsureValid(const fs::path &srcPath) {
    ValidateImage(srcPath);
}

void Image::ConvertJacket(const fs::path &srcPath, const fs::path &dstPath) {
    const RgbaImage img = LoadResizedRgba(srcPath, 300, 300);
    SaveJacketDds(img.span(), img.width, img.height, dstPath);
}

void Image::ConvertStage(const fs::path &bgSrcPath, const fs::path &stSrcPath, const fs::path &stDstPath,
                         const std::array<fs::path, 4> &fxSrcPaths) {

    auto stFut = std::async(std::launch::async, [&] { return ReadFileData(stSrcPath); });
    auto bgFut = std::async(std::launch::async, [&] { return ConvertBackground(bgSrcPath); });
    auto fxFut = std::async(std::launch::async, [&] { return ConvertEffect(fxSrcPaths); });

    const auto stAfb = stFut.get();
    const auto bgDds = bgFut.get();
    const auto fxDds = fxFut.get();

    const auto stChunks = LocateDdsChunks(stAfb);
    ReplaceChunks(stAfb, stDstPath, stChunks, {std::span<const uint8_t>(bgDds), std::span<const uint8_t>(fxDds)});
}

void Image::ExtractDds(const fs::path &srcPath, const fs::path &dstFolder) {
    std::vector<uint8_t> data = ReadFileData(srcPath);
    const auto chunks = LocateDdsChunks(data);
    if (chunks.empty()) {
        throw lib::FileError(srcPath, "No DDS chunks found");
    }
    auto baseName = srcPath.stem();
    if (baseName.empty()) {
        baseName = fs::path("chunk");
    }
    ExtractChunks(data, dstFolder, baseName, ".dds", chunks);
}
