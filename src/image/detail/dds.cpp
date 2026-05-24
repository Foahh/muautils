// src/image/detail/dds.cpp
#include "dds.hpp"

#include <DirectXTex.h>

#include <cstdint>
#include <cstring>
#include <fmt/format.h>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace Image::detail {

namespace {

[[nodiscard]] DXGI_FORMAT ToDxgiFormat(const DdsCompression compression) {
    switch (compression) {
    case DdsCompression::Bc1:
        return DXGI_FORMAT_BC1_UNORM;
    case DdsCompression::Bc3:
        return DXGI_FORMAT_BC3_UNORM;
    }
    throw std::runtime_error(fmt::format("Unsupported DDS compression {}", static_cast<int>(compression)));
}

void CheckDx(const HRESULT hr, const std::string_view operation, const unsigned width, const unsigned height,
             const DdsCompression compression) {
    if (FAILED(hr)) {
        throw std::runtime_error(fmt::format("{} failed for {}x{} DDS (compression={}): HRESULT 0x{:08X}",
                                             operation, width, height, static_cast<int>(compression),
                                             static_cast<uint32_t>(hr)));
    }
}

} // namespace

std::vector<uint8_t> EncodeDds(std::span<const uint8_t> rgba, const unsigned width, const unsigned height,
                               const DdsCompression compression) {
    const size_t expected = static_cast<size_t>(width) * height * 4;
    if (rgba.size() != expected) {
        throw std::runtime_error(fmt::format("RGBA buffer size mismatch: got {} bytes, expected {} for {}x{}",
                                             rgba.size(), expected, width, height));
    }

    static_assert(sizeof(RgbaPixel) == 4, "RgbaPixel must be 4 bytes for memcpy staging");
    RgbaImage image{.width = width, .height = height, .pixels = std::vector<RgbaPixel>(expected / 4)};
    std::memcpy(image.pixels.data(), rgba.data(), expected);
    return EncodeDds(std::move(image), compression);
}

std::vector<uint8_t> EncodeDds(RgbaImage image, const DdsCompression compression) {
    const size_t expected = static_cast<size_t>(image.width) * image.height;
    if (image.pixels.size() != expected) {
        throw std::runtime_error(fmt::format("RGBA buffer size mismatch: got {} pixels, expected {} for {}x{}",
                                             image.pixels.size(), expected, image.width, image.height));
    }

    DirectX::Image src{};
    src.width = image.width;
    src.height = image.height;
    src.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    src.rowPitch = static_cast<size_t>(image.width) * sizeof(RgbaPixel);
    src.slicePitch = src.rowPitch * image.height;
    src.pixels = reinterpret_cast<uint8_t *>(image.pixels.data());

    DirectX::ScratchImage compressed;
    HRESULT hr = DirectX::Compress(src, ToDxgiFormat(compression), DirectX::TEX_COMPRESS_PARALLEL,
                                   DirectX::TEX_THRESHOLD_DEFAULT, compressed);
    CheckDx(hr, "DirectXTex compression", image.width, image.height, compression);

    DirectX::Blob dds;
    hr = DirectX::SaveToDDSMemory(compressed.GetImages(), compressed.GetImageCount(), compressed.GetMetadata(),
                                  DirectX::DDS_FLAGS_NONE, dds);
    CheckDx(hr, "DirectXTex DDS serialization", image.width, image.height, compression);

    const uint8_t *bytes = dds.GetConstBufferPointer();
    return {bytes, bytes + dds.GetBufferSize()};
}

void SaveDds(const fs::path &dstPath, std::span<const uint8_t> bytes) {
    std::ofstream out(dstPath, std::ios::binary);
    if (!out)
        throw lib::FileError(dstPath, "Failed to create DDS file");
    out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!out)
        throw lib::FileError(dstPath, "Failed to write DDS file");
}

} // namespace Image::detail
