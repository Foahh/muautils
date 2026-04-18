// src/image/detail/vips.cpp
#include "vips.hpp"

#include <cstring>
#include <glib.h>

namespace Image::detail {

std::string PathToVipsUtf8(const fs::path &path) {
    const std::u8string u8 = path.u8string();
    return {reinterpret_cast<const char *>(u8.data()), u8.size()};
}

vips::VImage LoadVipsImage(const fs::path &path) {
    try {
        return vips::VImage::new_from_file(
            PathToVipsUtf8(path).c_str(),
            vips::VImage::option()->set("access", VIPS_ACCESS_SEQUENTIAL)->set("fail_on", VIPS_FAIL_ON_ERROR));
    } catch (const vips::VError &) {
        throw lib::FileError(path, "Failed to load image");
    }
}

vips::VImage ToRgbaUchar(vips::VImage img) {
    img = img.colourspace(VIPS_INTERPRETATION_sRGB);
    if (img.bands() == 1) {
        const vips::VImage g = img;
        img = g.bandjoin(g).bandjoin(g).bandjoin_const({255.0});
    } else if (img.bands() == 2) {
        const vips::VImage g = img.extract_band(0);
        const vips::VImage a = img.extract_band(1);
        img = g.bandjoin(g).bandjoin(g).bandjoin(a);
    } else if (img.bands() == 3) {
        img = img.bandjoin_const({255.0});
    } else if (img.bands() > 4) {
        img = img.extract_band(0, vips::VImage::option()->set("n", 4));
    }
    return img.cast(VIPS_FORMAT_UCHAR);
}

vips::VImage LanczosResizeTo(vips::VImage img, const int w, const int h) {
    const double xscale = static_cast<double>(w) / img.width();
    const double yscale = static_cast<double>(h) / img.height();
    return img.resize(xscale, vips::VImage::option()->set("vscale", yscale)->set("kernel", VIPS_KERNEL_LANCZOS3));
}

std::vector<uint8_t> RgbaPixelsFrom(const vips::VImage &img) {
    size_t sz = 0;
    void *buf = img.write_to_memory(&sz);
    std::vector<uint8_t> out(sz);
    std::memcpy(out.data(), buf, sz);
    g_free(buf);
    return out;
}

} // namespace Image::detail
