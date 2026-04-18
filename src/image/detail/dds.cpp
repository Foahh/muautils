// src/image/detail/dds.cpp
#include "dds.hpp"

#include "vips.hpp"

#include <cstring>
#include <fmt/format.h>
#include <fstream>

#include <rdo_bc_encoder.h>

namespace Image::detail {

namespace {

[[nodiscard]] uint32_t LegacyFourCc(const DXGI_FORMAT format) {
    switch (format) {
        case DXGI_FORMAT_BC1_UNORM:
            return PIXEL_FMT_FOURCC('D', 'X', 'T', '1');
        case DXGI_FORMAT_BC3_UNORM:
            return PIXEL_FMT_FOURCC('D', 'X', 'T', '5');
        default:
            throw std::runtime_error(fmt::format(
                "Legacy DDS writer received unsupported DXGI format {}",
                static_cast<int>(format)));
    }
}

[[nodiscard]] std::vector<uint8_t> EncodeToLegacyDds(std::vector<uint8_t> rgba,
                                                       const unsigned width,
                                                       const unsigned height,
                                                       const DXGI_FORMAT format) {
    const size_t expected = static_cast<size_t>(width) * height * 4;
    if (rgba.size() != expected) {
        throw std::runtime_error(fmt::format(
            "RGBA buffer size mismatch: got {} bytes, expected {} for {}x{}",
            rgba.size(), expected, width, height));
    }

    static_assert(sizeof(utils::color_quad_u8) == 4,
                  "utils::color_quad_u8 must be 4 bytes for memcpy staging");
    utils::image_u8 src;
    src.init(width, height);
    std::memcpy(src.get_pixels().data(), rgba.data(), expected);

    rdo_bc::rdo_bc_params params;
    params.m_dxgi_format = format;
    params.m_status_output = false;
    params.m_rdo_lambda = 0.0f;
    params.m_bc1_quality_level = rgbcx::MAX_LEVEL;
    params.m_use_hq_bc345 = true;
    params.m_rdo_multithreading = true;
    params.m_use_bc7e = false;

    rdo_bc::rdo_bc_encoder encoder;
    if (!encoder.init(src, params) || !encoder.encode()) {
        throw std::runtime_error(fmt::format(
            "bc7enc_rdo failed to encode {}x{} (format={})",
            width, height, static_cast<int>(format)));
    }

    DDSURFACEDESC2 desc{};
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_LINEARSIZE;
    desc.dwWidth = encoder.get_orig_width();
    desc.dwHeight = encoder.get_orig_height();
    desc.ddsCaps.dwCaps = DDSCAPS_TEXTURE;
    desc.ddpfPixelFormat.dwSize = sizeof(desc.ddpfPixelFormat);
    desc.ddpfPixelFormat.dwFlags = DDPF_FOURCC;
    desc.ddpfPixelFormat.dwFourCC = LegacyFourCc(encoder.get_pixel_format());
    desc.lPitch = static_cast<int32_t>(
        (((desc.dwWidth + 3u) & ~3u) * ((desc.dwHeight + 3u) & ~3u)
         * encoder.get_pixel_format_bpp()) >> 3);

    constexpr size_t headerBytes = 4 + sizeof(DDSURFACEDESC2);
    const size_t blockBytes = encoder.get_total_blocks_size_in_bytes();
    std::vector<uint8_t> out(headerBytes + blockBytes);
    std::memcpy(out.data(), "DDS ", 4);
    std::memcpy(out.data() + 4, &desc, sizeof(desc));
    std::memcpy(out.data() + headerBytes, encoder.get_blocks(), blockBytes);
    return out;
}

} // namespace

std::vector<uint8_t> ConvertBackground(const fs::path &srcPath) {
    vips::VImage img = ToRgbaUchar(LoadVipsImage(srcPath));
    img = LanczosResizeTo(std::move(img), 1920, 1080);
    const unsigned w = img.width();
    const unsigned h = img.height();
    return EncodeToLegacyDds(RgbaPixelsFrom(img), w, h, DXGI_FORMAT_BC1_UNORM);
}

std::vector<uint8_t> ConvertEffect(const std::array<fs::path, 4> &srcPaths) {
    constexpr int tileSize = 256;
    constexpr int canvasSize = tileSize * 2;
    vips::VImage canvas = vips::VImage::black(canvasSize, canvasSize,
                                               vips::VImage::option()->set("bands", 4));
    canvas = canvas.cast(VIPS_FORMAT_UCHAR);
    canvas = canvas.copy(vips::VImage::option()->set("interpretation", VIPS_INTERPRETATION_sRGB));
    for (int i = 0; i < 4; ++i) {
        if (srcPaths[i].empty()) continue;
        vips::VImage tile = ToRgbaUchar(LoadVipsImage(srcPaths[i]));
        tile = LanczosResizeTo(std::move(tile), tileSize, tileSize);
        canvas = canvas.insert(tile, (i % 2) * tileSize, (i / 2) * tileSize);
    }
    const unsigned w = canvas.width();
    const unsigned h = canvas.height();
    return EncodeToLegacyDds(RgbaPixelsFrom(canvas), w, h, DXGI_FORMAT_BC3_UNORM);
}

void SaveJacketDds(std::vector<uint8_t> rgba, const unsigned width, const unsigned height,
                   const fs::path &dstPath) {
    const auto blob = EncodeToLegacyDds(std::move(rgba), width, height, DXGI_FORMAT_BC1_UNORM);
    std::ofstream out(dstPath, std::ios::binary);
    if (!out) throw lib::FileError(dstPath, "Failed to create DDS file");
    out.write(reinterpret_cast<const char *>(blob.data()),
              static_cast<std::streamsize>(blob.size()));
    if (!out) throw lib::FileError(dstPath, "Failed to write DDS file");
}

} // namespace Image::detail
