// src/audio/detail/error.hpp
#pragma once

extern "C" {
#include <libavutil/error.h>
}

#include "lib.hpp"
#include <fmt/format.h>
#include <stdexcept>

namespace av {

template <typename... Args>
void Check(const int err, const fs::path &path, fmt::format_string<Args...> msg_fmt, Args &&...args) {
    if (err < 0) {
        char txt[1024];
        av_strerror(err, txt, sizeof(txt));
        auto msg = fmt::format(msg_fmt, std::forward<Args>(args)...);
        auto fmsg = fmt::format("{} (ffmpeg: {})", msg, txt);
        throw lib::FileError(path, fmsg);
    }
}

template <typename... Args> void Check(const int err, fmt::format_string<Args...> msg_fmt, Args &&...args) {
    if (err < 0) {
        char txt[1024];
        av_strerror(err, txt, sizeof(txt));
        auto msg = fmt::format(msg_fmt, std::forward<Args>(args)...);
        auto fmsg = fmt::format("{} (ffmpeg: {})", msg, txt);
        throw std::runtime_error(fmsg);
    }
}

template <typename... Args> void Require(const bool cond, fmt::format_string<Args...> msg_fmt, Args &&...args) {
    if (!cond) {
        const auto msg = fmt::format(msg_fmt, std::forward<Args>(args)...);
        throw std::runtime_error(msg);
    }
}

} // namespace av
