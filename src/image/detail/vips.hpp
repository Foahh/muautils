// src/image/detail/vips.hpp
#pragma once

#include "lib.hpp"

#include <cstdint>
#include <memory>
#include <span>

#include <vips/vips8>

namespace Image::detail {

struct GFreeDeleter {
    void operator()(void *p) const noexcept;
};

class VipsPixelBuffer {
  public:
    VipsPixelBuffer() = default;
    VipsPixelBuffer(uint8_t *data, size_t size) : m_data(data), m_size(size) {
    }

    [[nodiscard]] const uint8_t *data() const noexcept {
        return m_data.get();
    }
    [[nodiscard]] size_t size() const noexcept {
        return m_size;
    }
    [[nodiscard]] std::span<const uint8_t> span() const noexcept {
        return {m_data.get(), m_size};
    }

  private:
    std::unique_ptr<uint8_t, GFreeDeleter> m_data;
    size_t m_size = 0;
};

vips::VImage ToRgbaUchar(vips::VImage img);

vips::VImage LoadShrunkRgba(const fs::path &path, int w, int h);

VipsPixelBuffer RgbaPixelsFrom(const vips::VImage &img);

} // namespace Image::detail
