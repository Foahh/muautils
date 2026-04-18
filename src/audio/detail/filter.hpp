// src/audio/detail/filter.hpp
#pragma once

#include "audio/detail/error.hpp"
#include "audio/detail/raii.hpp"

extern "C" {
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
}

#include <fmt/format.h>

namespace Audio::detail {

AVFilterContext *Filter(const AVFilterGraphPtr &graph, const char *name, const char *instance);

AVFilterContext *Filter(const AVFilterGraphPtr &graph, AVFilterContext *from, const char *name, const char *instance,
                        const char *opts = nullptr);

template <typename... Args>
AVFilterContext *Filter(const AVFilterGraphPtr &graph, AVFilterContext *from, const char *name, const char *instance,
                        fmt::format_string<Args...> fmt, Args &&...args) {
    const auto opts = fmt::format(fmt, std::forward<Args>(args)...);
    return Filter(graph, from, name, instance, opts.c_str());
}

AVFilterContext *BufferSource(const AVFilterGraphPtr &graph, const AVCodecContextPtr &codec);

} // namespace Audio::detail
