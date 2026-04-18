// src/image/image.cpp
#include "image.hpp"

#include <fmt/format.h>
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
        vips::VImage::new_from_file(PathToVipsUtf8(srcPath).c_str(),
                                    vips::VImage::option()->set("fail_on", VIPS_FAIL_ON_WARNING));
    } catch (const vips::VError &e) {
        throw lib::FileError(srcPath, fmt::format("Invalid image: {}", e.what()));
    }
}

void Image::ConvertJacket(const fs::path &srcPath, const fs::path &dstPath) {
    vips::VImage img = ToRgbaUchar(LoadVipsImage(srcPath));
    img = LanczosResizeTo(std::move(img), 300, 300);
    const unsigned w = img.width();
    const unsigned h = img.height();
    auto rgba = RgbaPixelsFrom(img);
    SaveJacketDds(std::move(rgba), w, h, dstPath);
}

void Image::ConvertStage(const fs::path &bgSrcPath, const fs::path &stSrcPath,
                          const fs::path &stDstPath,
                          const std::array<fs::path, 4> &fxSrcPaths) {
    const auto stAfb = ReadFileData(stSrcPath);
    const auto bgDds = ConvertBackground(bgSrcPath);
    const auto fxDds = ConvertEffect(fxSrcPaths);

    const auto stChunks = LocateDdsChunks(stAfb);
    ReplaceChunks(stAfb, stDstPath, stChunks,
                  {std::span<const uint8_t>(bgDds), std::span<const uint8_t>(fxDds)});
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
