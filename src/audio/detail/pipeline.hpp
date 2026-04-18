// src/audio/detail/pipeline.hpp
#pragma once

#include "audio/detail/raii.hpp"

#include <cstdint>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
}

#include <functional>

namespace Audio::detail {

using OnFrame = std::function<void(AVFrame *)>;

// Reads the input through the decoder, pushes decoded frames into the filter graph
// rooted at `src`, and invokes `onFrame` for each frame produced at `sink`. Handles
// decoder flush and filter-graph flush. Caller is responsible for any encoder flush
// and trailer write that comes after.
void RunGraph(const AVFormatInputContextPtr &input,
              const AVStream *stream,
              const AVCodecContextPtr &decoder,
              AVFilterContext *src,
              AVFilterContext *sink,
              const OnFrame &onFrame);

void Encode(const AVFramePtr &frame,
            const AVCodecContextPtr &encoder,
            const AVFormatOutputContextPtr &output,
            const AVPacketPtr &pkt,
            AVRational src_frame_time_base);

} // namespace Audio::detail
