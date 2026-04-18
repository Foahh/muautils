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
#include "audio/detail/filter.hpp"
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

using detail::Filter;
using detail::BufferSource;
}

namespace Audio {

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
