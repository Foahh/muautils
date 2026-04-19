#pragma once

#include "raster.hpp"

#include <cstddef>
#include <span>
#include <string>

#include <turbojpeg.h>

namespace Image::detail {

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
    void operator()(void *handle) const noexcept;
};

[[nodiscard]] size_t PixelBufferSize(unsigned width, unsigned height);
[[nodiscard]] size_t PixelOffset(unsigned width, unsigned x, unsigned y);

[[nodiscard]] EncodedImageFormat DetectFormat(std::span<const uint8_t> bytes);
[[noreturn]] void ThrowDecodeError(const fs::path &path, const std::string &message);

[[nodiscard]] EncodedImageInfo InspectPng(const fs::path &path, std::span<const uint8_t> bytes);
[[nodiscard]] EncodedImageInfo InspectJpeg(const fs::path &path, std::span<const uint8_t> bytes);
[[nodiscard]] EncodedImageInfo InspectWebp(const fs::path &path, std::span<const uint8_t> bytes);
[[nodiscard]] EncodedImageInfo InspectImage(const fs::path &path, std::span<const uint8_t> bytes);

[[nodiscard]] bool CanDownscaleDuringDecode(const EncodedImageInfo &info, unsigned targetWidth, unsigned targetHeight);
[[nodiscard]] tjscalingfactor ChooseJpegScalingFactor(const EncodedImageInfo &info, unsigned targetWidth,
                                                      unsigned targetHeight);

[[nodiscard]] RgbaImage DecodePng(const fs::path &path, std::span<const uint8_t> bytes);
[[nodiscard]] RgbaImage DecodeJpeg(const fs::path &path, std::span<const uint8_t> bytes, const EncodedImageInfo &info);
[[nodiscard]] RgbaImage DecodeJpegScaled(const fs::path &path, std::span<const uint8_t> bytes,
                                         const EncodedImageInfo &info, unsigned targetWidth, unsigned targetHeight);
[[nodiscard]] RgbaImage DecodeWebp(const fs::path &path, std::span<const uint8_t> bytes, const EncodedImageInfo &info);
[[nodiscard]] RgbaImage DecodeWebpScaled(const fs::path &path, std::span<const uint8_t> bytes, unsigned targetWidth,
                                         unsigned targetHeight);

[[nodiscard]] RgbaImage ResizeRgba(const RgbaImage &src, unsigned dstWidth, unsigned dstHeight);

} // namespace Image::detail
