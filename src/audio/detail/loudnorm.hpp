// src/audio/detail/loudnorm.hpp
#pragma once

#include "audio/detail/raii.hpp"

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
                             double offset);

AVFilterContext *ApplyLoudNorm(const AVFilterGraphPtr &graph, AVFilterContext *from, const LoudNormStats &stats);

LoudNormStats AnalyzeLoudNorm(const AVFormatInputContextPtr &ifmt, const AVStream *ist, const AVCodecContextPtr &dctx,
                              double offset);

} // namespace Audio::detail
