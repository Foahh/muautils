#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
}

#include <fmt/format.h>
#include <memory>

#include "audio.hpp"
#include "audio/detail/error.hpp"
#include "audio/detail/format.hpp"
#include "audio/detail/raii.hpp"
#include "lib.hpp"
#include "utils.hpp"

namespace Audio {
using detail::AVCharPtr;
using detail::AVFormatInputContextPtr;
using detail::AVFormatOutputContextPtr;
using detail::AVCodecContextPtr;
using detail::AVFilterGraphPtr;
using detail::AVPacketPtr;
using detail::AVFramePtr;

using detail::OpenAVFormatInput;
using detail::OpenAVFormatOutput;
using detail::GetBestAudioStream;
using detail::OpenDecoder;
using detail::OpenEncoder;
using detail::OpenOutputStream;
}

namespace Audio {

inline AVFilterContext *Filter(const AVFilterGraphPtr &graph, const char *name, const char *instance) {
    const AVFilter *filt = avfilter_get_by_name(name);
    av::Ensure(filt, "Filter not found: {}", name);
    AVFilterContext *ctx = avfilter_graph_alloc_filter(graph.get(), filt, instance);
    av::Ensure(ctx, "Failed to allocate filter context");
    return ctx;
}

inline AVFilterContext *Filter(const AVFilterGraphPtr &graph, AVFilterContext *from, const char *name,
                               const char *instance, const char *opts = nullptr) {
    AVFilterContext *ctx = Filter(graph, name, instance);

    auto ret = avfilter_init_str(ctx, opts);
    av::Assert(ret, "Failed to initialize filter: {}", name);

    ret = avfilter_link(from, 0, ctx, 0);
    av::Assert(ret, "Failed to link filter: {}", name);

    return ctx;
}

template <typename... Args>
AVFilterContext *Filter(const AVFilterGraphPtr &graph, AVFilterContext *from, const char *name, const char *instance,
                        fmt::format_string<Args...> fmt, Args &&...args) {
    const auto opts = fmt::format(fmt, std::forward<Args>(args)...);
    return Filter(graph, from, name, instance, opts.c_str());
}

inline AVFilterContext *BufferSource(const AVFilterGraphPtr &graph, const AVCodecContextPtr &codec) {
    AVFilterContext *src = Filter(graph, "abuffer", "in");
    AVBufferSrcParameters *par = av_buffersrc_parameters_alloc();
    av::Ensure(par, "Failed to allocate buffer source parameters");

    par->format = codec->sample_fmt;
    par->sample_rate = codec->sample_rate;
    av_channel_layout_copy(&par->ch_layout, &codec->ch_layout);
    par->time_base = codec->time_base;

    auto ret = av_buffersrc_parameters_set(src, par);
    av::Assert(ret, "Failed to set parameters for buffer source: {}", src->filter->name);

    ret = avfilter_init_str(src, nullptr);
    av::Assert(ret, "Failed to initialize buffer source: {}", src->filter->name);

    av_freep(&par);
    return src;
}

inline void Encode(const AVFramePtr &frm, const AVCodecContextPtr &ectx, const AVFormatOutputContextPtr &ofmt,
                   const AVPacketPtr &pkt, const AVStream *ist) {
    if (frm->pts != AV_NOPTS_VALUE) {
        frm->pts = av_rescale_q(frm->pts, ist->time_base, ectx->time_base);
    }

    auto ret = avcodec_send_frame(ectx.get(), frm.get());
    av::Assert(ret, "Failed to send frame to encoder: {}", ectx->codec->name);

    while (avcodec_receive_packet(ectx.get(), pkt.get()) == 0) {
        ret = av_interleaved_write_frame(ofmt.get(), pkt.get());
        av::Assert(ret, "Failed to write packet to output format: {}", ofmt->oformat->name);
        av_packet_unref(pkt.get());
    }
}

inline void Decode(const AVCodecContextPtr &dctx, AVFilterContext *fsrc, AVFilterContext *fsnk, const AVFramePtr &dfrm,
                   const AVFramePtr &ffrm) {
    for (;;) {
        auto ret = avcodec_receive_frame(dctx.get(), dfrm.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        av::Assert(ret, "Failed to receive frame from decoder");

        ret = av_buffersrc_add_frame(fsrc, dfrm.get());
        av::Assert(ret, "Failed to add frame to buffer source: {}", fsrc->filter->name);
        av_frame_unref(dfrm.get());

        while (av_buffersink_get_frame(fsnk, ffrm.get()) == 0) {
            av_frame_unref(ffrm.get());
        }
    }
}

inline void DecodeEncode(AVFilterContext *fsrc, AVFilterContext *fsnk, const AVCodecContextPtr &dctx,
                         const AVStream *ist, const AVCodecContextPtr &ectx, const AVFormatOutputContextPtr &ofmt,
                         const AVPacketPtr &pkt, const AVFramePtr &dfrm, const AVFramePtr &ffrm) {
    for (;;) {
        auto ret = avcodec_receive_frame(dctx.get(), dfrm.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        av::Assert(ret, "Failed to receive frame from decoder");

        ret = av_buffersrc_add_frame(fsrc, dfrm.get());
        av::Assert(ret, "Failed to add frame to buffer source: {}", fsrc->filter->name);
        av_frame_unref(dfrm.get());

        while (av_buffersink_get_frame(fsnk, ffrm.get()) == 0) {
            Encode(ffrm, ectx, ofmt, pkt, ist);
            av_frame_unref(ffrm.get());
        }
    }
}

} // namespace Audio
