#include "raster.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imageio.h>

namespace Image::detail {
namespace {

[[nodiscard]] size_t PixelBufferSize(const unsigned width, const unsigned height) {
    return static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
}

[[nodiscard]] size_t PixelOffset(const unsigned width, const unsigned x, const unsigned y) {
    return (static_cast<size_t>(y) * width + x) * 4;
}

[[noreturn]] void ThrowImageError(const fs::path &path, const std::string &message) {
    throw lib::FileError(path, message);
}

void ValidateImageSpec(const fs::path &path, const OIIO::ImageSpec &spec) {
    if (spec.width <= 0 || spec.height <= 0) {
        ThrowImageError(path, "Invalid image dimensions");
    }
    if (spec.nchannels <= 0) {
        ThrowImageError(path, "Invalid image channel count");
    }
}

[[nodiscard]] OIIO::ImageBuf LoadOrientedImage(const fs::path &path) {
    OIIO::ImageBuf image(lib::PathToUtf8(path));
    if (!image.read(0, 0, true, OIIO::TypeDesc::UINT8)) {
        ThrowImageError(path, image.geterror());
    }

    OIIO::ImageBuf oriented = OIIO::ImageBufAlgo::reorient(image);
    if (oriented.has_error()) {
        ThrowImageError(path, oriented.geterror());
    }
    return oriented;
}

[[nodiscard]] RgbaImage ToRgbaImage(const fs::path &path, OIIO::ImageBuf &image) {
    const OIIO::ImageSpec &spec = image.spec();
    ValidateImageSpec(path, spec);

    const int channels = std::max(1, spec.nchannels);
    OIIO::ROI roi = OIIO::get_roi(spec);
    roi.chbegin = 0;
    roi.chend = channels;

    const auto width = static_cast<unsigned>(spec.width);
    const auto height = static_cast<unsigned>(spec.height);
    std::vector<uint8_t> source(static_cast<size_t>(spec.width) * spec.height * channels);
    if (!image.get_pixels(roi, OIIO::TypeDesc::UINT8, source.data())) {
        ThrowImageError(path, image.geterror());
    }

    RgbaImage rgba{.width = width, .height = height, .pixels = std::vector<uint8_t>(PixelBufferSize(width, height))};
    for (size_t pixel = 0; pixel < static_cast<size_t>(width) * height; ++pixel) {
        const uint8_t *src = source.data() + pixel * channels;
        uint8_t *dst = rgba.pixels.data() + pixel * 4;
        if (channels == 1) {
            dst[0] = src[0];
            dst[1] = src[0];
            dst[2] = src[0];
            dst[3] = 255;
        } else if (channels == 2) {
            dst[0] = src[0];
            dst[1] = src[0];
            dst[2] = src[0];
            dst[3] = src[1];
        } else {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = channels >= 4 ? src[3] : 255;
        }
    }
    return rgba;
}

} // namespace

void ValidateImage(const fs::path &path) {
    auto input = OIIO::ImageInput::open(lib::PathToUtf8(path));
    if (!input) {
        std::string message = OIIO::geterror();
        if (message.empty()) {
            message = "Failed to open image";
        }
        ThrowImageError(path, message);
    }
    ValidateImageSpec(path, input->spec());
}

RgbaImage LoadResizedRgba(const fs::path &path, const int width, const int height) {
    if (width <= 0 || height <= 0) {
        throw lib::FileError(path, "Requested image size must be positive");
    }

    OIIO::ImageBuf image = LoadOrientedImage(path);
    const OIIO::ROI roi(0, width, 0, height, 0, 1, 0, image.nchannels());
    OIIO::ImageBuf resized = OIIO::ImageBufAlgo::resize(image, {}, roi);
    if (resized.has_error()) {
        ThrowImageError(path, resized.geterror());
    }
    return ToRgbaImage(path, resized);
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
