// src/audio/detail/loudnorm.hpp
#pragma once

#include "audio/detail/raii.hpp"
#include "audio/detail/target_format.hpp"

extern "C" {
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
}

namespace Audio::detail {

struct LoudNormStats {
    double InputI = 0.0;
    double InputTP = 0.0;
    double InputLRA = 0.0;
    double InputThresh = 0.0;
    double TargetOffset = 0.0;
};

AVFilterContext *ApplyOffset(const AVFilterGraphPtr &graph, const AVCodecContextPtr &dctx, AVFilterContext *from,
                             double offset, const TargetFormat &target);

AVFilterContext *ApplyLoudNorm(const AVFilterGraphPtr &graph, AVFilterContext *from, const LoudNormStats &stats,
                               const TargetFormat &target);

LoudNormStats AnalyzeLoudNorm(const AVFormatInputContextPtr &ifmt, const AVStream *ist, const AVCodecContextPtr &dctx,
                              double offset, const TargetFormat &target);

} // namespace Audio::detail
