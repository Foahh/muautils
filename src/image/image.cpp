// src/image/image.cpp
#include "image.hpp"

#include "detail/chunk.hpp"
#include "detail/dds.hpp"
#include "detail/raster.hpp"

using namespace Image::detail;

namespace {

[[nodiscard]] std::vector<uint8_t> ConvertJacketDds(const fs::path &srcPath) {
    return EncodeDds(LoadResizedRgba(srcPath, 300, 300), DdsCompression::Bc1);
}

[[nodiscard]] std::vector<uint8_t> ConvertBackgroundDds(const fs::path &srcPath) {
    return EncodeDds(LoadResizedRgba(srcPath, 1920, 1080), DdsCompression::Bc1);
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

    return EncodeDds(JoinTiles2x2(tiles), DdsCompression::Bc3);
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
    const auto stAfb = ReadFileData(stSrcPath);
    const auto bgDds = ConvertBackgroundDds(bgSrcPath);
    const auto fxDds = ConvertEffectDds(fxSrcPaths);

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
