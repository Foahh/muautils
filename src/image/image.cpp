#include "image.hpp"
#include "lib.hpp"
#include "utils.hpp"

#include <spdlog/spdlog.h>

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
    FreeImage_Initialise();
    FreeImage_SetOutputMessage([](FREE_IMAGE_FORMAT, const char *msg) {
        spdlog::error(msg);
    });
}

void Image::EnsureValid(const fs::path &srcPath) {
    if (!IsImageValid(srcPath)) {
        throw lib::FileError(srcPath, "Invalid image format or unsupported file type");
    }
}