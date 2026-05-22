// src/image/detail/dds.cpp
#include "dds.hpp"

#include <algorithm>
#include <cstring>
#include <fmt/format.h>
#include <fstream>

#include <rdo_bc_encoder.h>

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

[[nodiscard]] uint32_t CompressionFourCc(const DXGI_FORMAT format) {
    switch (format) {
    case DXGI_FORMAT_BC1_UNORM:
        return PIXEL_FMT_FOURCC('D', 'X', 'T', '1');
    case DXGI_FORMAT_BC3_UNORM:
        return PIXEL_FMT_FOURCC('D', 'X', 'T', '5');
    default:
        throw std::runtime_error(
            fmt::format("DDS writer received unsupported DXGI format {}", static_cast<int>(format)));
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

    static_assert(sizeof(utils::color_quad_u8) == 4, "utils::color_quad_u8 must be 4 bytes for memcpy staging");
    utils::image_u8 src;
    src.init(width, height);
    std::memcpy(src.get_pixels().data(), rgba.data(), expected);

    rdo_bc::rdo_bc_params params;
    params.m_dxgi_format = ToDxgiFormat(compression);
    params.m_status_output = false;
    params.m_rdo_lambda = 0.0f;
    params.m_bc1_quality_level = rgbcx::MAX_LEVEL;
    params.m_use_hq_bc345 = true;
    params.m_rdo_multithreading = true;
    params.m_use_bc7e = false;

    rdo_bc::rdo_bc_encoder encoder;
    if (!encoder.init(src, params) || !encoder.encode()) {
        throw std::runtime_error(fmt::format("bc7enc_rdo failed to encode {}x{} (compression={})", width, height,
                                             static_cast<int>(compression)));
    }

    DDSURFACEDESC2 desc{};
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_LINEARSIZE;
    desc.dwWidth = encoder.get_orig_width();
    desc.dwHeight = encoder.get_orig_height();
    desc.ddsCaps.dwCaps = DDSCAPS_TEXTURE;
    desc.ddpfPixelFormat.dwSize = sizeof(desc.ddpfPixelFormat);
    desc.ddpfPixelFormat.dwFlags = DDPF_FOURCC;
    desc.ddpfPixelFormat.dwFourCC = CompressionFourCc(encoder.get_pixel_format());
    const uint32_t blocksWide = std::max(1u, (desc.dwWidth + 3u) / 4u);
    const uint32_t blocksTall = std::max(1u, (desc.dwHeight + 3u) / 4u);
    const uint32_t bytesPerBlock = encoder.get_pixel_format_bpp() * 2u;
    desc.lPitch = static_cast<int32_t>(blocksWide * blocksTall * bytesPerBlock);

    constexpr size_t headerBytes = 4 + sizeof(DDSURFACEDESC2);
    const size_t compressedBytes = encoder.get_total_blocks_size_in_bytes();
    std::vector<uint8_t> out(headerBytes + compressedBytes);
    std::memcpy(out.data(), "DDS ", 4);
    std::memcpy(out.data() + 4, &desc, sizeof(desc));
    std::memcpy(out.data() + headerBytes, encoder.get_blocks(), compressedBytes);
    return out;
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
