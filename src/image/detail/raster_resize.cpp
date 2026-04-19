#include "raster_internal.hpp"

#include "chunk.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace Image::detail {

RgbaImage ResizeRgba(const RgbaImage &src, const unsigned dstWidth, const unsigned dstHeight) {
    if (src.width == 0 || src.height == 0) {
        throw std::runtime_error("Source image is empty");
    }
    if (dstWidth == 0 || dstHeight == 0) {
        throw std::runtime_error("Destination image is empty");
    }
    if (src.width == dstWidth && src.height == dstHeight) {
        return src;
    }

    RgbaImage resized{.width = dstWidth,
                      .height = dstHeight,
                      .pixels = std::vector<uint8_t>(PixelBufferSize(dstWidth, dstHeight))};

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
