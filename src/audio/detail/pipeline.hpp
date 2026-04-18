// src/audio/detail/pipeline.hpp
#pragma once

#include "audio/detail/error.hpp"
#include "audio/detail/raii.hpp"

#include <utility>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
}

namespace Audio::detail {

namespace pipeline_detail {

template <typename FrameCb>
void pumpDecoder(const AVCodecContextPtr &decoder,
                 AVFilterContext *src,
                 AVFilterContext *sink,
                 const AVFramePtr &dfrm,
                 const AVFramePtr &ffrm,
                 FrameCb &&cb) {
    for (;;) {
        auto ret = avcodec_receive_frame(decoder.get(), dfrm.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
        av::Check(ret, "Failed to receive frame from decoder");

        ret = av_buffersrc_add_frame(src, dfrm.get());
        av::Check(ret, "Failed to add frame to buffer source: {}", src->filter->name);
        av_frame_unref(dfrm.get());

        while (av_buffersink_get_frame(sink, ffrm.get()) == 0) {
            cb(ffrm.get());
            av_frame_unref(ffrm.get());
        }
    }
}

} // namespace pipeline_detail

// Reads the input through the decoder, pushes decoded frames into the filter graph
// rooted at `src`, and invokes `onFrame` for each frame produced at `sink`. Handles
// decoder flush and filter-graph flush. Caller is responsible for any encoder flush
// and trailer write that comes after.
template <typename OnFrame>
void RunGraph(const AVFormatInputContextPtr &input,
              const AVStream *stream,
              const AVCodecContextPtr &decoder,
              AVFilterContext *src,
              AVFilterContext *sink,
              OnFrame &&onFrame) {
    auto &&cb = std::forward<OnFrame>(onFrame);

    const AVPacketPtr pkt(av_packet_alloc());
    const AVFramePtr dfrm(av_frame_alloc());
    const AVFramePtr ffrm(av_frame_alloc());
    av::Require(pkt && dfrm && ffrm, "Failed to allocate packet or frame");

    while (av_read_frame(input.get(), pkt.get()) >= 0) {
        if (pkt->stream_index != stream->index) {
            av_packet_unref(pkt.get());
            continue;
        }
        auto ret = avcodec_send_packet(decoder.get(), pkt.get());
        av::Check(ret, "Failed to send packet to decoder: {}", decoder->codec->name);
        av_packet_unref(pkt.get());
        pipeline_detail::pumpDecoder(decoder, src, sink, dfrm, ffrm, cb);
    }

    auto ret = avcodec_send_packet(decoder.get(), nullptr);
    av::Check(ret, "Failed to send end-of-stream packet to decoder: {}", decoder->codec->name);
    pipeline_detail::pumpDecoder(decoder, src, sink, dfrm, ffrm, cb);

    ret = av_buffersrc_add_frame(src, nullptr);
    av::Check(ret, "Failed to add end-of-stream frame to buffer source: {}", src->filter->name);

    while (av_buffersink_get_frame(sink, ffrm.get()) == 0) {
        cb(ffrm.get());
        av_frame_unref(ffrm.get());
    }
}

void Encode(const AVFramePtr &frame,
            const AVCodecContextPtr &encoder,
            const AVFormatOutputContextPtr &output,
            const AVPacketPtr &pkt,
            AVRational src_frame_time_base);

} // namespace Audio::detail
