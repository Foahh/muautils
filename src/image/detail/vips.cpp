// src/image/detail/vips.cpp
#include "vips.hpp"

#include <glib.h>

namespace Image::detail {

void GFreeDeleter::operator()(void *p) const noexcept {
    if (p)
        g_free(p);
}

vips::VImage ToRgbaUchar(vips::VImage img) {
    if (img.interpretation() != VIPS_INTERPRETATION_sRGB) {
        img = img.colourspace(VIPS_INTERPRETATION_sRGB);
    }
    if (!img.has_alpha()) {
        img = img.bandjoin_const({255.0});
    }
    if (img.format() != VIPS_FORMAT_UCHAR) {
        img = img.cast(VIPS_FORMAT_UCHAR);
    }
    return img;
}

vips::VImage LoadShrunkRgba(const fs::path &path, const int w, const int h) {
    try {
        vips::VImage img = vips::VImage::thumbnail(lib::PathToUtf8(path).c_str(), w,
                                                   vips::VImage::option()
                                                       ->set("height", h)
                                                       ->set("size", VIPS_SIZE_FORCE)
                                                       ->set("no_rotate", true)
                                                       ->set("fail_on", VIPS_FAIL_ON_ERROR));
        return ToRgbaUchar(std::move(img));
    } catch (const vips::VError &) {
        throw lib::FileError(path, "Failed to load image");
    }
}

VipsPixelBuffer RgbaPixelsFrom(const vips::VImage &img) {
    size_t sz = 0;
    void *buf = img.write_to_memory(&sz);
    return {static_cast<uint8_t *>(buf), sz};
}

} // namespace Image::detail
