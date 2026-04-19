#include "raster_internal.hpp"

#include <cstring>
#include <memory>

#include <png.h>
#include <turbojpeg.h>
#include <webp/decode.h>

namespace Image::detail {

RgbaImage DecodePng(const fs::path &path, const std::span<const uint8_t> bytes) {
    png_image image{};
    image.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_memory(&image, bytes.data(), bytes.size())) {
        ThrowDecodeError(path, image.message ? image.message : "Failed to parse PNG");
    }

    image.format = PNG_FORMAT_RGBA;
    RgbaImage decoded{.width = image.width,
                      .height = image.height,
                      .pixels = std::vector<uint8_t>(PixelBufferSize(image.width, image.height))};
    if (!png_image_finish_read(&image, nullptr, decoded.pixels.data(), 0, nullptr)) {
        const std::string message = image.message ? image.message : "Failed to decode PNG";
        png_image_free(&image);
        ThrowDecodeError(path, message);
    }
    png_image_free(&image);
    return decoded;
}

RgbaImage DecodeJpeg(const fs::path &path, const std::span<const uint8_t> bytes, const EncodedImageInfo &info) {
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

RgbaImage DecodeJpegScaled(const fs::path &path, const std::span<const uint8_t> bytes, const EncodedImageInfo &info,
                           const unsigned targetWidth, const unsigned targetHeight) {
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

RgbaImage DecodeWebp(const fs::path &path, const std::span<const uint8_t> bytes, const EncodedImageInfo &info) {
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

RgbaImage DecodeWebpScaled(const fs::path &path, const std::span<const uint8_t> bytes, const unsigned targetWidth,
                           const unsigned targetHeight) {
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

} // namespace Image::detail
