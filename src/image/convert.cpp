#include <array>
#include <filesystem>
#include <optional>
#include <span>
#include <fmt/format.h>

#include "lib.hpp"
#include "utils.hpp"

#include <DirectXTex.h>

namespace Image {

template <typename... Args>
void Assert(const HRESULT hr, fmt::format_string<Args...> msg_fmt, Args &&... args) {
    if (FAILED(hr)) {
        auto msg = fmt::format(msg_fmt, std::forward<Args>(args)...);
        auto m_msg = fmt::format("{} (HRESULT: 0x{:08X})", msg, hr);
        throw std::runtime_error(m_msg);
    }
}

template <typename... Args>
void Assert(const HRESULT hr, const fs::path &path, fmt::format_string<Args...> msg_fmt, Args &&... args) {
    if (FAILED(hr)) {
        auto msg = fmt::format(msg_fmt, std::forward<Args>(args)...);
        auto fmsg = fmt::format("{} [hr = 0x{:08X}]", msg, hr);
        throw lib::FileError(path, fmsg);
    }
}

class BlockImage {
public:
    explicit BlockImage(const fipImage &img, const DXGI_FORMAT format) {
        constexpr size_t channels = 4;
        const size_t width = img.getWidth();
        const size_t height = img.getHeight();
        const size_t rowPitch = img.getScanWidth();

        DirectX::Image dxi{};
        dxi.width = width;
        dxi.height = height;
        dxi.format = DXGI_FORMAT_R8G8B8A8_UNORM;
        dxi.rowPitch = rowPitch;
        dxi.slicePitch = height * rowPitch;

        std::vector<uint8_t> rgba(width * height * channels);
        const BYTE *srcBits = img.accessPixels();

        for (size_t y = 0; y < height; ++y) {
            const size_t flippedY = height - 1 - y;
            const BYTE *srcRow = srcBits + flippedY * rowPitch;
            for (size_t x = 0; x < width; ++x) {
                const BYTE *src = srcRow + x * channels;
                const size_t idx = channels * (y * width + x);
                // BGRA to RGBA
                rgba[idx + 0] = src[2];
                rgba[idx + 1] = src[1];
                rgba[idx + 2] = src[0];
                rgba[idx + 3] = src[3];
            }
        }

        dxi.pixels = rgba.data();

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

    [[nodiscard]] const DirectX::ScratchImage &GetScratchImage() const {
        return m_scratch;
    }

private:
    DirectX::ScratchImage m_scratch;
};

inline DirectX::Blob ConvertBackground(const fs::path &srcPath) {
    auto bgImg = LoadFip32Image(srcPath);
    if (!bgImg.rescale(1920, 1080, FILTER_LANCZOS3)) {
        throw lib::FileError(srcPath, "Failed to rescale background image");
    }
    return BlockImage(bgImg, DXGI_FORMAT_BC1_UNORM).SaveToMemory();
}

inline DirectX::Blob ConvertEffect(const std::array<fs::path, 4> &srcPaths) {
    constexpr int tileSize = 256;
    constexpr int canvasSize = tileSize * 2;

    fipImage canvas(FIT_BITMAP, canvasSize, canvasSize, 32);

    for (int i = 0; i < 4; ++i) {
        if (srcPaths[i].empty()) {
            continue;
        }

        auto img = LoadFip32Image(srcPaths[i]);
        if (!img.rescale(tileSize, tileSize, FILTER_LANCZOS3)) {
            throw lib::FileError(srcPaths[i], "Failed to rescale effect tile image");
        }

        const int x = i % 2 * tileSize;
        const int y = i / 2 * tileSize;

        if (!canvas.pasteSubImage(img, x, y)) {
            throw lib::FileError(srcPaths[i], fmt::format("Failed to paste effect tile at ({}, {})", x, y));
        }
    }
    return BlockImage(canvas, DXGI_FORMAT_BC3_UNORM).SaveToMemory();
}

void ConvertJacket(const fs::path &srcPath, const fs::path &dstPath) {
    auto img = LoadFip32Image(srcPath);
    if (!img.rescale(300, 300, FILTER_LANCZOS3)) {
        throw lib::FileError(srcPath, "Failed to rescale jacket image");
    }
    BlockImage(img, DXGI_FORMAT_BC1_UNORM).Save(dstPath);
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