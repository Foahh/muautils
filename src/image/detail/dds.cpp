// src/image/detail/dds.cpp
#include "dds.hpp"

#include "vips.hpp"

#include <fmt/format.h>

namespace Image::detail {

namespace {

template <typename... Args>
void Assert(const HRESULT hr, fmt::format_string<Args...> msg_fmt, Args &&...args) {
    if (FAILED(hr)) {
        throw std::runtime_error(
            fmt::format("{} (HRESULT: 0x{:08X})", fmt::format(msg_fmt, std::forward<Args>(args)...), hr));
    }
}

template <typename... Args>
void Assert(const HRESULT hr, const fs::path &path, fmt::format_string<Args...> msg_fmt, Args &&...args) {
    if (FAILED(hr)) {
        throw lib::FileError(
            path, fmt::format("{} [hr = 0x{:08X}]", fmt::format(msg_fmt, std::forward<Args>(args)...), hr));
    }
}

class BlockImage {
  public:
    explicit BlockImage(std::vector<uint8_t> rgba, unsigned width, unsigned height, DXGI_FORMAT format)
        : m_rgba(std::move(rgba)) {
        constexpr unsigned channels = 4;
        const size_t rowPitch = width * channels;
        DirectX::Image dxi{};
        dxi.width = width;
        dxi.height = height;
        dxi.format = DXGI_FORMAT_R8G8B8A8_UNORM;
        dxi.rowPitch = rowPitch;
        dxi.slicePitch = height * rowPitch;
        dxi.pixels = m_rgba.data();
        DirectX::ScratchImage scratch;
        Assert(DirectX::Compress(dxi, format, DirectX::TEX_COMPRESS_DEFAULT, 0.5f, scratch),
               "Failed to compress image ({}x{})", dxi.width, dxi.height);
        m_scratch = std::move(scratch);
    }

    [[nodiscard]] DirectX::Blob SaveToMemory() const {
        DirectX::Blob blob;
        Assert(DirectX::SaveToDDSMemory(m_scratch.GetImages(), m_scratch.GetImageCount(),
                                        m_scratch.GetMetadata(), DirectX::DDS_FLAGS_FORCE_DX9_LEGACY, blob),
               "Failed to save DDS image to memory");
        return blob;
    }

    void Save(const fs::path &dstPath) const {
        Assert(DirectX::SaveToDDSFile(m_scratch.GetImages(), m_scratch.GetImageCount(),
                                      m_scratch.GetMetadata(), DirectX::DDS_FLAGS_FORCE_DX9_LEGACY,
                                      dstPath.wstring().c_str()),
               dstPath, "Failed to save DDS image");
    }

  private:
    std::vector<uint8_t> m_rgba;
    DirectX::ScratchImage m_scratch;
};

} // namespace

DirectX::Blob ConvertBackground(const fs::path &srcPath) {
    vips::VImage img = ToRgbaUchar(LoadVipsImage(srcPath));
    img = LanczosResizeTo(std::move(img), 1920, 1080);
    const unsigned w = img.width();
    const unsigned h = img.height();
    auto rgba = RgbaPixelsFrom(img);
    return BlockImage(std::move(rgba), w, h, DXGI_FORMAT_BC1_UNORM).SaveToMemory();
}

DirectX::Blob ConvertEffect(const std::array<fs::path, 4> &srcPaths) {
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
    auto rgba = RgbaPixelsFrom(canvas);
    return BlockImage(std::move(rgba), w, h, DXGI_FORMAT_BC3_UNORM).SaveToMemory();
}

void SaveJacketDds(std::vector<uint8_t> rgba, const unsigned width, const unsigned height,
                   const fs::path &dstPath) {
    BlockImage(std::move(rgba), width, height, DXGI_FORMAT_BC1_UNORM).Save(dstPath);
}

} // namespace Image::detail
