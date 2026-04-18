#include "image.hpp"
#include "lib.hpp"
#include "utils.hpp"

#include <fmt/format.h>
#include <vips/vips8>

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

void Image::Initialize() {
    if (vips_init("mua") != 0) {
        throw std::runtime_error("Failed to initialize libvips");
    }
}

void Image::EnsureValid(const fs::path &srcPath) {
    try {
        vips::VImage::new_from_file(srcPath.u8string().c_str(),
                                    vips::VImage::option()->set("fail_on", VIPS_FAIL_ON_WARNING));
    } catch (const vips::VError &e) {
        throw lib::FileError(srcPath, fmt::format("Invalid image: {}", e.what()));
    }
}
