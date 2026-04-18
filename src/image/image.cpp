// src/image/image.cpp
#include "image.hpp"

#include <fmt/format.h>
#include <future>
#include <vips/vips8>

#include "detail/chunk.hpp"
#include "detail/dds.hpp"
#include "detail/vips.hpp"

using namespace Image::detail;

void Image::Initialize() {
    if (vips_init("mua") != 0) {
        throw std::runtime_error("Failed to initialize libvips");
    }
}

void Image::EnsureValid(const fs::path &srcPath) {
    try {
        vips::VImage::new_from_file(lib::PathToUtf8(srcPath).c_str(),
                                    vips::VImage::option()->set("fail_on", VIPS_FAIL_ON_WARNING));
    } catch (const vips::VError &e) {
        throw lib::FileError(srcPath, fmt::format("Invalid image: {}", e.what()));
    }
}

void Image::ConvertJacket(const fs::path &srcPath, const fs::path &dstPath) {
    vips::VImage img = LoadShrunkRgba(srcPath, 300, 300);
    const unsigned w = img.width();
    const unsigned h = img.height();
    const auto pixels = RgbaPixelsFrom(img);
    SaveJacketDds(pixels.span(), w, h, dstPath);
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
