// src/image/image.cpp
#include "image.hpp"

#include <future>

#include "detail/chunk.hpp"
#include "detail/dds.hpp"
#include "detail/raster.hpp"

using namespace Image::detail;

namespace {

[[nodiscard]] std::vector<uint8_t> ConvertJacketDds(const fs::path &srcPath) {
    const RgbaImage img = LoadResizedRgba(srcPath, 300, 300);
    return EncodeDds(img.span(), img.width, img.height, DdsCompression::Bc1);
}

[[nodiscard]] std::vector<uint8_t> ConvertBackgroundDds(const fs::path &srcPath) {
    const RgbaImage img = LoadResizedRgba(srcPath, 1920, 1080);
    return EncodeDds(img.span(), img.width, img.height, DdsCompression::Bc1);
}

[[nodiscard]] std::vector<uint8_t> ConvertEffectDds(const std::array<fs::path, 4> &srcPaths) {
    constexpr int tileSize = 256;

    std::array<RgbaImage, 4> tiles;
    for (int i = 0; i < 4; ++i) {
        if (srcPaths[i].empty()) {
            tiles[i] = MakeBlankRgba(tileSize, tileSize);
        } else {
            tiles[i] = LoadResizedRgba(srcPaths[i], tileSize, tileSize);
        }
    }

    const RgbaImage canvas = JoinTiles2x2(tiles);
    return EncodeDds(canvas.span(), canvas.width, canvas.height, DdsCompression::Bc3);
}

} // namespace

void Image::Initialize() {
    // no-op
}

void Image::EnsureValid(const fs::path &srcPath) {
    ValidateImage(srcPath);
}

void Image::ConvertJacket(const fs::path &srcPath, const fs::path &dstPath) {
    const auto dds = ConvertJacketDds(srcPath);
    SaveDds(dstPath, dds);
}

void Image::ConvertStage(const fs::path &bgSrcPath, const fs::path &stSrcPath, const fs::path &stDstPath,
                         const std::array<fs::path, 4> &fxSrcPaths) {

    auto stFut = std::async(std::launch::async, [&] { return ReadFileData(stSrcPath); });
    auto bgFut = std::async(std::launch::async, [&] { return ConvertBackgroundDds(bgSrcPath); });
    auto fxFut = std::async(std::launch::async, [&] { return ConvertEffectDds(fxSrcPaths); });

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
