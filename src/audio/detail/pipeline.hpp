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
void pumpDecoder(const AVCodecContextPtr &decoder, AVFilterContext *src, AVFilterContext *sink, const AVFramePtr &dfrm,
                 const AVFramePtr &ffrm, FrameCb &&cb) {
    for (;;) {
        auto ret = avcodec_receive_frame(decoder.get(), dfrm.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
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
void RunGraph(const AVFormatInputContextPtr &input, const AVStream *stream, const AVCodecContextPtr &decoder,
              AVFilterContext *src, AVFilterContext *sink, OnFrame &&onFrame) {
    auto &&cb = std::forward<OnFrame>(onFrame);

    const AVPacketPtr pkt(av_packet_alloc());
    const AVFramePtr dfrm(av_frame_alloc());
    const AVFramePtr ffrm(av_frame_alloc());
    av::Require(pkt && dfrm && ffrm, "Failed to allocate packet or frame");

    for (;;) {
        const auto rret = av_read_frame(input.get(), pkt.get());
        if (rret == AVERROR_EOF)
            break;
        av::Check(rret, "Failed to read frame from input");

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

// Writes one encoded packet to `output`, rescaling timestamps from `encoder`'s time_base
// to `ost`'s time_base (which the muxer may have rewritten during avformat_write_header)
// and tagging the packet with the correct output stream index. Unrefs the packet on success.
void WritePacket(const AVPacketPtr &pkt, const AVCodecContextPtr &encoder, const AVFormatOutputContextPtr &output,
                 const AVStream *ost);

// Drains any packets the encoder currently has queued, forwarding them through WritePacket.
// Stops on AVERROR(EAGAIN); propagates any other error.
void DrainEncoder(const AVCodecContextPtr &encoder, const AVFormatOutputContextPtr &output, const AVStream *ost,
                  const AVPacketPtr &pkt);

// Pushes `frame` into `encoder` and drains any packets it produces. `src_frame_time_base`
// is the time_base of `frame->pts` (typically the filter graph's abuffersink time_base).
void Encode(const AVFramePtr &frame, const AVCodecContextPtr &encoder, const AVFormatOutputContextPtr &output,
            const AVStream *ost, const AVPacketPtr &pkt, AVRational src_frame_time_base);

// Signals end-of-stream to `encoder` and drains remaining packets through WritePacket.
void FlushEncoder(const AVCodecContextPtr &encoder, const AVFormatOutputContextPtr &output, const AVStream *ost,
                  const AVPacketPtr &pkt);

} // namespace Audio::detail
