#include "raster_internal.hpp"

#include <algorithm>
#include <array>
#include <memory>
#include <stdexcept>

#include <png.h>
#include <turbojpeg.h>
#include <webp/decode.h>

namespace Image::detail {

void TurboJpegDeleter::operator()(void *handle) const noexcept {
    if (handle != nullptr) {
        tjDestroy(handle);
    }
}

size_t PixelBufferSize(const unsigned width, const unsigned height) {
    return static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
}

size_t PixelOffset(const unsigned width, const unsigned x, const unsigned y) {
    return (static_cast<size_t>(y) * width + x) * 4;
}

EncodedImageFormat DetectFormat(const std::span<const uint8_t> bytes) {
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

EncodedImageInfo InspectPng(const fs::path &path, const std::span<const uint8_t> bytes) {
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

EncodedImageInfo InspectJpeg(const fs::path &path, const std::span<const uint8_t> bytes) {
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

EncodedImageInfo InspectWebp(const fs::path &path, const std::span<const uint8_t> bytes) {
    int width = 0;
    int height = 0;
    if (!WebPGetInfo(bytes.data(), bytes.size(), &width, &height) || width <= 0 || height <= 0) {
        ThrowDecodeError(path, "Invalid WebP image");
    }

    return {.format = EncodedImageFormat::Webp, .width = static_cast<unsigned>(width),
            .height = static_cast<unsigned>(height)};
}

EncodedImageInfo InspectImage(const fs::path &path, const std::span<const uint8_t> bytes) {
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

bool CanDownscaleDuringDecode(const EncodedImageInfo &info, const unsigned targetWidth, const unsigned targetHeight) {
    return targetWidth > 0 && targetHeight > 0 && targetWidth <= info.width && targetHeight <= info.height;
}

tjscalingfactor ChooseJpegScalingFactor(const EncodedImageInfo &info, const unsigned targetWidth,
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

} // namespace Image::detail
