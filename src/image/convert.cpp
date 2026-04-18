#include <array>
#include <cstring>
#include <filesystem>
#include <span>
#include <vector>

#include <glib.h>
#include <vips/vips8>

#include <fmt/format.h>

#include <DirectXTex.h>

#include "lib.hpp"
#include "utils.hpp"

namespace Image {

namespace {

using vips::VError;
using vips::VImage;

template <typename... Args> void Assert(const HRESULT hr, fmt::format_string<Args...> msg_fmt, Args &&...args) {
    if (FAILED(hr)) {
        auto msg = fmt::format(msg_fmt, std::forward<Args>(args)...);
        auto m_msg = fmt::format("{} (HRESULT: 0x{:08X})", msg, hr);
        throw std::runtime_error(m_msg);
    }
}

template <typename... Args>
void Assert(const HRESULT hr, const fs::path &path, fmt::format_string<Args...> msg_fmt, Args &&...args) {
    if (FAILED(hr)) {
        auto msg = fmt::format(msg_fmt, std::forward<Args>(args)...);
        auto fmsg = fmt::format("{} [hr = 0x{:08X}]", msg, hr);
        throw lib::FileError(path, fmsg);
    }
}

VImage LoadVipsImage(const fs::path &path) {
    try {
        return VImage::new_from_file(
            lib::PathToUtf8(path).c_str(),
            VImage::option()->set("access", VIPS_ACCESS_SEQUENTIAL)->set("fail_on", VIPS_FAIL_ON_ERROR));
    } catch (const VError &) {
        throw lib::FileError(path, "Failed to load image");
    }
}

VImage ToRgbaUchar(VImage img) {
    img = img.colourspace(VIPS_INTERPRETATION_sRGB);

    if (img.bands() == 1) {
        const VImage g = img;
        img = g.bandjoin(g).bandjoin(g).bandjoin_const({255.0});
    } else if (img.bands() == 2) {
        const VImage g = img.extract_band(0);
        const VImage a = img.extract_band(1);
        img = g.bandjoin(g).bandjoin(g).bandjoin(a);
    } else if (img.bands() == 3) {
        img = img.bandjoin_const({255.0});
    } else if (img.bands() > 4) {
        img = img.extract_band(0, VImage::option()->set("n", 4));
    }

    return img.cast(VIPS_FORMAT_UCHAR);
}

VImage LanczosResizeTo(VImage img, const int w, const int h) {
    const double xscale = static_cast<double>(w) / img.width();
    const double yscale = static_cast<double>(h) / img.height();
    return img.resize(xscale, VImage::option()->set("vscale", yscale)->set("kernel", VIPS_KERNEL_LANCZOS3));
}

std::vector<uint8_t> RgbaPixelsFrom(const VImage &img) {
    size_t sz = 0;
    void *buf = img.write_to_memory(&sz);
    std::vector<uint8_t> out(sz);
    std::memcpy(out.data(), buf, sz);
    g_free(buf);
    return out;
}

class BlockImage {
  public:
    explicit BlockImage(std::vector<uint8_t> rgba, const unsigned width, const unsigned height,
                        const DXGI_FORMAT format)
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
        const auto hr = DirectX::Compress(dxi, format, DirectX::TEX_COMPRESS_DEFAULT, 0.5f, scratch);
        Assert(hr, "Failed to compress image ({}x{})", dxi.width, dxi.height);

        m_scratch = std::move(scratch);
    }

    [[nodiscard]] DirectX::Blob SaveToMemory() const {
        DirectX::Blob blob;
        const auto hr = DirectX::SaveToDDSMemory(m_scratch.GetImages(), m_scratch.GetImageCount(),
                                                 m_scratch.GetMetadata(), DirectX::DDS_FLAGS_FORCE_DX9_LEGACY, blob);
        Assert(hr, "Failed to save DDS image to memory");
        return blob;
    }

    void Save(const fs::path &dstPath) const {
        const auto hr =
            DirectX::SaveToDDSFile(m_scratch.GetImages(), m_scratch.GetImageCount(), m_scratch.GetMetadata(),
                                   DirectX::DDS_FLAGS_FORCE_DX9_LEGACY, dstPath.wstring().c_str());
        Assert(hr, dstPath, "Failed to save DDS image");
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

    vips::VImage canvas = vips::VImage::black(canvasSize, canvasSize, vips::VImage::option()->set("bands", 4));
    canvas = canvas.cast(VIPS_FORMAT_UCHAR);
    canvas = canvas.copy(vips::VImage::option()->set("interpretation", VIPS_INTERPRETATION_sRGB));

    for (int i = 0; i < 4; ++i) {
        if (srcPaths[i].empty()) {
            continue;
        }

        vips::VImage tile = ToRgbaUchar(LoadVipsImage(srcPaths[i]));
        tile = LanczosResizeTo(std::move(tile), tileSize, tileSize);

        const int x = (i % 2) * tileSize;
        const int y = (i / 2) * tileSize;
        canvas = canvas.insert(tile, x, y);
    }

    const unsigned w = canvas.width();
    const unsigned h = canvas.height();
    auto rgba = RgbaPixelsFrom(canvas);
    return BlockImage(std::move(rgba), w, h, DXGI_FORMAT_BC3_UNORM).SaveToMemory();
}

void ConvertJacket(const fs::path &srcPath, const fs::path &dstPath) {
    vips::VImage img = ToRgbaUchar(LoadVipsImage(srcPath));
    img = LanczosResizeTo(std::move(img), 300, 300);
    const unsigned w = img.width();
    const unsigned h = img.height();
    auto rgba = RgbaPixelsFrom(img);
    BlockImage(std::move(rgba), w, h, DXGI_FORMAT_BC1_UNORM).Save(dstPath);
}

void ConvertStage(const fs::path &bgSrcPath, const fs::path &stSrcPath, const fs::path &stDstPath,
                  const std::array<fs::path, 4> &fxSrcPaths) {
    const auto stAfb = ReadFileData(stSrcPath);
    const auto bgBlob = ConvertBackground(bgSrcPath);
    const auto fxBlob = ConvertEffect(fxSrcPaths);

    const auto bgDds = std::span(bgBlob.GetBufferPointer(), bgBlob.GetBufferPointer() + bgBlob.GetBufferSize());
    const auto fxDds = std::span(fxBlob.GetBufferPointer(), fxBlob.GetBufferPointer() + fxBlob.GetBufferSize());

    const auto stChunks = LocateDdsChunks(stAfb);
    ReplaceChunks(stAfb, stDstPath, stChunks, {bgDds, fxDds});
}

} // namespace Image
