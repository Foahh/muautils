#include "raster.hpp"

#include "chunk.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

#include <png.h>
#include <turbojpeg.h>
#include <webp/decode.h>

namespace Image::detail {

namespace {

enum class EncodedImageFormat {
    Jpeg,
    Png,
    Webp
};

struct EncodedImageInfo {
    EncodedImageFormat format;
    unsigned width;
    unsigned height;
};

struct TurboJpegDeleter {
    void operator()(void *handle) const noexcept {
        if (handle != nullptr) {
            tjDestroy(handle);
        }
    }
};

[[nodiscard]] size_t PixelBufferSize(const unsigned width, const unsigned height) {
    return static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
}

[[nodiscard]] size_t PixelOffset(const unsigned width, const unsigned x, const unsigned y) {
    return (static_cast<size_t>(y) * width + x) * 4;
}

[[nodiscard]] EncodedImageFormat DetectFormat(const std::span<const uint8_t> bytes) {
    if (bytes.size() >= 12 && bytes[0] == 'R' && bytes[1] == 'I' && bytes[2] == 'F' && bytes[3] == 'F' &&
        bytes[8] == 'W' && bytes[9] == 'E' && bytes[10] == 'B' && bytes[11] == 'P') {
        return EncodedImageFormat::Webp;
    }
    constexpr std::array<uint8_t, 8> pngSignature = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    if (bytes.size() >= pngSignature.size() && std::equal(pngSignature.begin(), pngSignature.end(), bytes.begin())) {
        return EncodedImageFormat::Png;
    }
    if (bytes.size() >= 3 && bytes[0] == 0xFF && bytes[1] == 0xD8 && bytes[2] == 0xFF) {
        return EncodedImageFormat::Jpeg;
    }
    throw std::runtime_error("Unsupported image format");
}

[[noreturn]] void ThrowDecodeError(const fs::path &path, const std::string &message) {
    throw lib::FileError(path, message);
}

[[nodiscard]] RgbaImage DecodePng(const fs::path &path, const std::span<const uint8_t> bytes);
[[nodiscard]] RgbaImage DecodeJpeg(const fs::path &path, const std::span<const uint8_t> bytes,
                                   const EncodedImageInfo &info);
[[nodiscard]] RgbaImage DecodeWebp(const fs::path &path, const std::span<const uint8_t> bytes,
                                   const EncodedImageInfo &info);
[[nodiscard]] RgbaImage DecodeJpegScaled(const fs::path &path, const std::span<const uint8_t> bytes,
                                         const EncodedImageInfo &info, unsigned targetWidth, unsigned targetHeight);
[[nodiscard]] RgbaImage DecodeWebpScaled(const fs::path &path, const std::span<const uint8_t> bytes,
                                         unsigned targetWidth, unsigned targetHeight);

[[nodiscard]] EncodedImageInfo InspectPng(const fs::path &path, const std::span<const uint8_t> bytes) {
    png_image image{};
    image.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_memory(&image, bytes.data(), bytes.size())) {
        ThrowDecodeError(path, image.message ? image.message : "Failed to parse PNG");
    }

    const EncodedImageInfo info{.format = EncodedImageFormat::Png, .width = image.width, .height = image.height};
    png_image_free(&image);
    if (info.width == 0 || info.height == 0) {
        ThrowDecodeError(path, "Invalid PNG dimensions");
    }
    return info;
}

[[nodiscard]] EncodedImageInfo InspectJpeg(const fs::path &path, const std::span<const uint8_t> bytes) {
    std::unique_ptr<void, TurboJpegDeleter> handle(tjInitDecompress());
    if (!handle) {
        ThrowDecodeError(path, "Failed to initialize JPEG decoder");
    }

    int width = 0;
    int height = 0;
    int subsamp = 0;
    int colorspace = 0;
    if (tjDecompressHeader3(handle.get(), bytes.data(), static_cast<unsigned long>(bytes.size()), &width, &height,
                            &subsamp, &colorspace) != 0) {
        ThrowDecodeError(path, tjGetErrorStr2(handle.get()));
    }
    if (width <= 0 || height <= 0) {
        ThrowDecodeError(path, "Invalid JPEG dimensions");
    }

    return {.format = EncodedImageFormat::Jpeg, .width = static_cast<unsigned>(width),
            .height = static_cast<unsigned>(height)};
}

[[nodiscard]] EncodedImageInfo InspectWebp(const fs::path &path, const std::span<const uint8_t> bytes) {
    int width = 0;
    int height = 0;
    if (!WebPGetInfo(bytes.data(), bytes.size(), &width, &height) || width <= 0 || height <= 0) {
        ThrowDecodeError(path, "Invalid WebP image");
    }

    return {.format = EncodedImageFormat::Webp, .width = static_cast<unsigned>(width),
            .height = static_cast<unsigned>(height)};
}

[[nodiscard]] EncodedImageInfo InspectImage(const fs::path &path, const std::span<const uint8_t> bytes) {
    switch (DetectFormat(bytes)) {
    case EncodedImageFormat::Jpeg:
        return InspectJpeg(path, bytes);
    case EncodedImageFormat::Png:
        return InspectPng(path, bytes);
    case EncodedImageFormat::Webp:
        return InspectWebp(path, bytes);
    }
    ThrowDecodeError(path, "Unsupported image format");
}

[[nodiscard]] bool CanDownscaleDuringDecode(const EncodedImageInfo &info, const unsigned targetWidth,
                                            const unsigned targetHeight) {
    return targetWidth > 0 && targetHeight > 0 && targetWidth <= info.width && targetHeight <= info.height;
}

[[nodiscard]] tjscalingfactor ChooseJpegScalingFactor(const EncodedImageInfo &info, const unsigned targetWidth,
                                                      const unsigned targetHeight) {
    if (!CanDownscaleDuringDecode(info, targetWidth, targetHeight)) {
        return TJUNSCALED;
    }

    int factorCount = 0;
    tjscalingfactor *factors = tjGetScalingFactors(&factorCount);
    if (factors == nullptr || factorCount <= 0) {
        return TJUNSCALED;
    }

    tjscalingfactor best = TJUNSCALED;
    size_t bestArea = PixelBufferSize(info.width, info.height);
    bool found = false;

    for (int i = 0; i < factorCount; ++i) {
        const unsigned scaledWidth = static_cast<unsigned>(TJSCALED(info.width, factors[i]));
        const unsigned scaledHeight = static_cast<unsigned>(TJSCALED(info.height, factors[i]));
        if (scaledWidth < targetWidth || scaledHeight < targetHeight) {
            continue;
        }

        const size_t area = static_cast<size_t>(scaledWidth) * scaledHeight;
        if (!found || area < bestArea) {
            best = factors[i];
            bestArea = area;
            found = true;
        }
    }

    return found ? best : TJUNSCALED;
}

[[nodiscard]] RgbaImage DecodePng(const fs::path &path, const std::span<const uint8_t> bytes) {
    png_image image{};
    image.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_memory(&image, bytes.data(), bytes.size())) {
        ThrowDecodeError(path, image.message ? image.message : "Failed to parse PNG");
    }

    image.format = PNG_FORMAT_RGBA;
    RgbaImage decoded{.width = image.width, .height = image.height, .pixels = std::vector<uint8_t>(
                          PixelBufferSize(image.width, image.height))};
    if (!png_image_finish_read(&image, nullptr, decoded.pixels.data(), 0, nullptr)) {
        const std::string message = image.message ? image.message : "Failed to decode PNG";
        png_image_free(&image);
        ThrowDecodeError(path, message);
    }
    png_image_free(&image);
    return decoded;
}

[[nodiscard]] RgbaImage DecodeJpeg(const fs::path &path, const std::span<const uint8_t> bytes,
                                   const EncodedImageInfo &info) {
    std::unique_ptr<void, TurboJpegDeleter> handle(tjInitDecompress());
    if (!handle) {
        ThrowDecodeError(path, "Failed to initialize JPEG decoder");
    }

    RgbaImage decoded{.width = info.width,
                      .height = info.height,
                      .pixels = std::vector<uint8_t>(PixelBufferSize(info.width, info.height))};
    if (tjDecompress2(handle.get(), bytes.data(), static_cast<unsigned long>(bytes.size()), decoded.pixels.data(),
                      static_cast<int>(info.width), 0, static_cast<int>(info.height), TJPF_RGBA,
                      TJFLAG_ACCURATEDCT) != 0) {
        ThrowDecodeError(path, tjGetErrorStr2(handle.get()));
    }
    return decoded;
}

[[nodiscard]] RgbaImage DecodeJpegScaled(const fs::path &path, const std::span<const uint8_t> bytes,
                                         const EncodedImageInfo &info, const unsigned targetWidth,
                                         const unsigned targetHeight) {
    const tjscalingfactor factor = ChooseJpegScalingFactor(info, targetWidth, targetHeight);
    if (factor.num == TJUNSCALED.num && factor.denom == TJUNSCALED.denom) {
        return DecodeJpeg(path, bytes, info);
    }

    std::unique_ptr<void, TurboJpegDeleter> handle(tjInitDecompress());
    if (!handle) {
        ThrowDecodeError(path, "Failed to initialize JPEG decoder");
    }

    const unsigned scaledWidth = static_cast<unsigned>(TJSCALED(info.width, factor));
    const unsigned scaledHeight = static_cast<unsigned>(TJSCALED(info.height, factor));
    RgbaImage decoded{.width = scaledWidth,
                      .height = scaledHeight,
                      .pixels = std::vector<uint8_t>(PixelBufferSize(scaledWidth, scaledHeight))};
    if (tjDecompress2(handle.get(), bytes.data(), static_cast<unsigned long>(bytes.size()), decoded.pixels.data(),
                      static_cast<int>(scaledWidth), 0, static_cast<int>(scaledHeight), TJPF_RGBA,
                      TJFLAG_FASTDCT) != 0) {
        ThrowDecodeError(path, tjGetErrorStr2(handle.get()));
    }
    return decoded;
}

[[nodiscard]] RgbaImage DecodeWebp(const fs::path &path, const std::span<const uint8_t> bytes,
                                   const EncodedImageInfo &info) {
    using WebpPtr = std::unique_ptr<uint8_t, decltype(&WebPFree)>;
    int width = static_cast<int>(info.width);
    int height = static_cast<int>(info.height);
    WebpPtr decodedBytes(WebPDecodeRGBA(bytes.data(), bytes.size(), &width, &height), &WebPFree);
    if (!decodedBytes) {
        ThrowDecodeError(path, "Failed to decode WebP image");
    }

    RgbaImage decoded{.width = static_cast<unsigned>(width),
                      .height = static_cast<unsigned>(height),
                      .pixels = std::vector<uint8_t>(PixelBufferSize(static_cast<unsigned>(width),
                                                                      static_cast<unsigned>(height)))};
    std::memcpy(decoded.pixels.data(), decodedBytes.get(), decoded.pixels.size());
    return decoded;
}

[[nodiscard]] RgbaImage DecodeWebpScaled(const fs::path &path, const std::span<const uint8_t> bytes,
                                         const unsigned targetWidth, const unsigned targetHeight) {
    WebPDecoderConfig config{};
    if (!WebPInitDecoderConfig(&config)) {
        ThrowDecodeError(path, "Failed to initialize WebP decoder");
    }
    if (WebPGetFeatures(bytes.data(), bytes.size(), &config.input) != VP8_STATUS_OK) {
        ThrowDecodeError(path, "Invalid WebP image");
    }

    config.options.use_scaling = 1;
    config.options.scaled_width = static_cast<int>(targetWidth);
    config.options.scaled_height = static_cast<int>(targetHeight);
    config.output.colorspace = MODE_RGBA;

    RgbaImage decoded{.width = targetWidth,
                      .height = targetHeight,
                      .pixels = std::vector<uint8_t>(PixelBufferSize(targetWidth, targetHeight))};
    config.output.is_external_memory = 1;
    config.output.u.RGBA.rgba = decoded.pixels.data();
    config.output.u.RGBA.stride = static_cast<int>(targetWidth * 4);
    config.output.u.RGBA.size = decoded.pixels.size();

    const VP8StatusCode status = WebPDecode(bytes.data(), bytes.size(), &config);
    WebPFreeDecBuffer(&config.output);
    if (status != VP8_STATUS_OK) {
        ThrowDecodeError(path, "Failed to decode WebP image");
    }
    return decoded;
}

[[nodiscard]] RgbaImage ResizeRgba(const RgbaImage &src, const unsigned dstWidth, const unsigned dstHeight) {
    if (src.width == 0 || src.height == 0) {
        throw std::runtime_error("Source image is empty");
    }
    if (dstWidth == 0 || dstHeight == 0) {
        throw std::runtime_error("Destination image is empty");
    }
    if (src.width == dstWidth && src.height == dstHeight) {
        return src;
    }

    RgbaImage resized{.width = dstWidth, .height = dstHeight, .pixels = std::vector<uint8_t>(PixelBufferSize(
                                                  dstWidth, dstHeight))};

    const double scaleX = static_cast<double>(src.width) / static_cast<double>(dstWidth);
    const double scaleY = static_cast<double>(src.height) / static_cast<double>(dstHeight);

    for (unsigned y = 0; y < dstHeight; ++y) {
        const double srcY = std::clamp((static_cast<double>(y) + 0.5) * scaleY - 0.5, 0.0,
                                       static_cast<double>(src.height - 1));
        const unsigned y0 = static_cast<unsigned>(srcY);
        const unsigned y1 = std::min(y0 + 1, src.height - 1);
        const double wy = srcY - static_cast<double>(y0);

        for (unsigned x = 0; x < dstWidth; ++x) {
            const double srcX = std::clamp((static_cast<double>(x) + 0.5) * scaleX - 0.5, 0.0,
                                           static_cast<double>(src.width - 1));
            const unsigned x0 = static_cast<unsigned>(srcX);
            const unsigned x1 = std::min(x0 + 1, src.width - 1);
            const double wx = srcX - static_cast<double>(x0);

            const size_t topLeft = PixelOffset(src.width, x0, y0);
            const size_t topRight = PixelOffset(src.width, x1, y0);
            const size_t bottomLeft = PixelOffset(src.width, x0, y1);
            const size_t bottomRight = PixelOffset(src.width, x1, y1);
            const size_t dst = PixelOffset(dstWidth, x, y);

            for (size_t channel = 0; channel < 4; ++channel) {
                const double top = static_cast<double>(src.pixels[topLeft + channel]) * (1.0 - wx) +
                                   static_cast<double>(src.pixels[topRight + channel]) * wx;
                const double bottom = static_cast<double>(src.pixels[bottomLeft + channel]) * (1.0 - wx) +
                                      static_cast<double>(src.pixels[bottomRight + channel]) * wx;
                resized.pixels[dst + channel] = static_cast<uint8_t>(
                    std::clamp(std::lround(top * (1.0 - wy) + bottom * wy), 0l, 255l));
            }
        }
    }

    return resized;
}

} // namespace

void ValidateImage(const fs::path &path) {
    const std::vector<uint8_t> bytes = ReadFileData(path);
    try {
        static_cast<void>(InspectImage(path, bytes));
    } catch (const lib::FileError &) {
        throw;
    } catch (const std::exception &e) {
        ThrowDecodeError(path, e.what());
    }
}

RgbaImage LoadRgba(const fs::path &path) {
    const std::vector<uint8_t> bytes = ReadFileData(path);
    try {
        const EncodedImageInfo info = InspectImage(path, bytes);
        switch (info.format) {
        case EncodedImageFormat::Jpeg:
            return DecodeJpeg(path, bytes, info);
        case EncodedImageFormat::Png:
            return DecodePng(path, bytes);
        case EncodedImageFormat::Webp:
            return DecodeWebp(path, bytes, info);
        }
    } catch (const lib::FileError &) {
        throw;
    } catch (const std::exception &e) {
        ThrowDecodeError(path, e.what());
    }
    ThrowDecodeError(path, "Unsupported image format");
}

RgbaImage LoadResizedRgba(const fs::path &path, const int width, const int height) {
    if (width <= 0 || height <= 0) {
        throw lib::FileError(path, "Requested image size must be positive");
    }

    const unsigned targetWidth = static_cast<unsigned>(width);
    const unsigned targetHeight = static_cast<unsigned>(height);
    const std::vector<uint8_t> bytes = ReadFileData(path);
    try {
        const EncodedImageInfo info = InspectImage(path, bytes);
        switch (info.format) {
        case EncodedImageFormat::Jpeg:
            return ResizeRgba(DecodeJpegScaled(path, bytes, info, targetWidth, targetHeight), targetWidth,
                              targetHeight);
        case EncodedImageFormat::Png:
            return ResizeRgba(DecodePng(path, bytes), targetWidth, targetHeight);
        case EncodedImageFormat::Webp:
            if (CanDownscaleDuringDecode(info, targetWidth, targetHeight)) {
                return DecodeWebpScaled(path, bytes, targetWidth, targetHeight);
            }
            return ResizeRgba(DecodeWebp(path, bytes, info), targetWidth, targetHeight);
        }
    } catch (const lib::FileError &) {
        throw;
    } catch (const std::exception &e) {
        ThrowDecodeError(path, e.what());
    }
    ThrowDecodeError(path, "Unsupported image format");
}

RgbaImage MakeBlankRgba(const unsigned width, const unsigned height) {
    if (width == 0 || height == 0) {
        throw std::runtime_error("Blank image dimensions must be positive");
    }
    return {.width = width, .height = height, .pixels = std::vector<uint8_t>(PixelBufferSize(width, height), 0)};
}

RgbaImage JoinTiles2x2(const std::array<RgbaImage, 4> &tiles) {
    const unsigned tileWidth = tiles[0].width;
    const unsigned tileHeight = tiles[0].height;
    for (const auto &tile : tiles) {
        if (tile.width != tileWidth || tile.height != tileHeight) {
            throw std::runtime_error("Effect tiles must have matching dimensions");
        }
        if (tile.pixels.size() != PixelBufferSize(tile.width, tile.height)) {
            throw std::runtime_error("Invalid RGBA tile buffer size");
        }
    }

    RgbaImage canvas = MakeBlankRgba(tileWidth * 2, tileHeight * 2);
    for (unsigned tileIndex = 0; tileIndex < tiles.size(); ++tileIndex) {
        const unsigned tileX = tileIndex % 2;
        const unsigned tileY = tileIndex / 2;
        const auto &tile = tiles[tileIndex];
        for (unsigned row = 0; row < tile.height; ++row) {
            const size_t src = PixelOffset(tile.width, 0, row);
            const size_t dst = PixelOffset(canvas.width, tileX * tileWidth, tileY * tileHeight + row);
            std::memcpy(canvas.pixels.data() + dst, tile.pixels.data() + src, static_cast<size_t>(tile.width) * 4);
        }
    }
    return canvas;
}

} // namespace Image::detail
