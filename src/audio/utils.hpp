#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavfilter/buffersink.h>
}

#include <memory>
#include <fmt/format.h>

#include "lib.hpp"
#include "audio.hpp"
#include "utils.hpp"


namespace av {
template <typename... Args>
void Assert(const int err, const fs::path& path, fmt::format_string<Args...> msg_fmt, Args&&... args) {
    if (err < 0) {
        char txt[1024];
        av_strerror(err, txt, sizeof(txt));
        auto msg = fmt::format(msg_fmt, std::forward<Args>(args)...);
        auto fmsg = fmt::format("{} (ffmpeg: {})", msg, txt);
        throw lib::FileError(path, fmsg);
    }
}

template <typename... Args>
void Assert(const int err, fmt::format_string<Args...> msg_fmt, Args&&... args) {
    if (err < 0) {
        char txt[1024];
        av_strerror(err, txt, sizeof(txt));
        auto msg = fmt::format(msg_fmt, std::forward<Args>(args)...);
        auto fmsg = fmt::format("{} (ffmpeg: {})", msg, txt);
        throw std::runtime_error(fmsg);
    }
}

template <typename... Args>
void Ensure(const bool cond, fmt::format_string<Args...> msg_fmt, Args &&... args) {
    if (!cond) {
        const auto msg = fmt::format(msg_fmt, std::forward<Args>(args)...);
        throw std::runtime_error(msg);
    }
}
}

namespace Audio {

// ffmpeg uses utf8 path on Windows
inline std::string str(const fs::path &path) {
#ifdef _WIN32
    auto s = path.u8string();
    return {s.begin(), s.end()};
#else
    return path.string();
#endif
}

struct AVCharDeleter {
    void operator()(char *ptr) const { av_freep(&ptr); }
};
using AVCharPtr = std::unique_ptr<char, AVCharDeleter>;

struct AVFormatInputContextDeleter {
    void operator()(AVFormatContext *ctx) const { avformat_close_input(&ctx); }
};
using AVFormatInputContextPtr = std::unique_ptr<AVFormatContext, AVFormatInputContextDeleter>;

struct AVFormatOutputContextDeleter {
    void operator()(AVFormatContext *ctx) const {
        if (!ctx) {
            return;
        }
        if (ctx->pb) {
            avio_closep(&ctx->pb);
        }
        avformat_free_context(ctx);
    }
};
using AVFormatOutputContextPtr = std::unique_ptr<AVFormatContext, AVFormatOutputContextDeleter>;

struct AVCodecContextDeleter {
    void operator()(AVCodecContext *ctx) const { avcodec_free_context(&ctx); }
};
using AVCodecContextPtr = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>;

struct AVFilterGraphDeleter {
    void operator()(AVFilterGraph *graph) const { avfilter_graph_free(&graph); }
};
using AVFilterGraphPtr = std::unique_ptr<AVFilterGraph, AVFilterGraphDeleter>;

struct AVPacketDeleter {
    void operator()(AVPacket *pkt) const { av_packet_free(&pkt); }
};
using AVPacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;

struct AVFrameDeleter {
    void operator()(AVFrame *frame) const { av_frame_free(&frame); }
};
using AVFramePtr = std::unique_ptr<AVFrame, AVFrameDeleter>;

inline AVFormatInputContextPtr OpenAVFormatInput(const fs::path &path) {
    AVFormatContext *raw = nullptr;
    auto ret = avformat_open_input(&raw, str(path).c_str(), nullptr, nullptr);
    av::Assert(ret, path, "Failed to open input format context");
    auto ctx = AVFormatInputContextPtr(raw);
    ret = avformat_find_stream_info(ctx.get(), nullptr);
    av::Assert(ret, path, "Failed to find stream info");
    return ctx;
}

inline AVFormatOutputContextPtr OpenAVFormatOutput(const fs::path &path) {
    AVFormatContext *raw = nullptr;
    const auto ret = avformat_alloc_output_context2(&raw, nullptr, "wav", str(path).c_str());
    av::Assert(ret, path, "Failed to allocate output format context");
    return AVFormatOutputContextPtr(raw);
}

inline AVStream *GetBestAudioStream(const AVFormatInputContextPtr &ctx) {
    const int ret = av_find_best_stream(ctx.get(), AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    av::Assert(ret, "No audio stream found in input format context");
    return ctx->streams[ret];
}

inline AVCodecContextPtr OpenDecoder(const AVStream *st) {
    const AVCodec *codec = avcodec_find_decoder(st->codecpar->codec_id);
    av::Ensure(codec, "Failed to find decoder for stream codec");

    AVCodecContext *raw = avcodec_alloc_context3(codec);
    av::Ensure(raw, "Failed to allocate decoder context");
    auto ctx = AVCodecContextPtr(raw);

    auto ret = avcodec_parameters_to_context(ctx.get(), st->codecpar);
    av::Assert(ret, "Failed to copy codec parameters to context");

    ret = avcodec_open2(ctx.get(), codec, nullptr);
    av::Assert(ret, "Failed to open decoder");

    if (ctx->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC || ctx->ch_layout.nb_channels == 0) {
        int ch = ctx->ch_layout.nb_channels;
        if (ch == 0) {
            ch = st->codecpar->ch_layout.nb_channels;
        }
        av::Ensure(ch > 0, "No audio channels available in stream");
        av_channel_layout_uninit(&ctx->ch_layout);
        av_channel_layout_default(&ctx->ch_layout, ch);
    }

    return ctx;
}

inline AVCodecContextPtr OpenEncoder(const NormalizeFormat &params) {
    auto ectx = AVCodecContextPtr(avcodec_alloc_context3(nullptr));
    av::Ensure(ectx.get(), "Failed to allocate encoder context");
    ectx->codec_type = AVMEDIA_TYPE_AUDIO;
    ectx->codec_id = params.CodecId;
    ectx->sample_rate = params.SampleRate;
    av_channel_layout_default(&ectx->ch_layout, 2);
    ectx->sample_fmt = params.SampleFormat;
    ectx->bit_rate = 0;
    ectx->time_base = AVRational{1, params.SampleRate};
    const auto ret = avcodec_open2(ectx.get(), avcodec_find_encoder(ectx->codec_id), nullptr);
    av::Assert(ret, "Failed to open encoder");
    return ectx;
}

inline AVStream *OpenOutputStream(const fs::path &path, const AVFormatOutputContextPtr &ofmt,
                                  const AVCodecContextPtr &ectx) {
    AVStream *ost = avformat_new_stream(ofmt.get(), nullptr);
    av::Ensure(ost, "Failed to create output stream");
    ost->time_base = ectx->time_base;

    auto ret = avcodec_parameters_from_context(ost->codecpar, ectx.get());
    av::Assert(ret, "Failed to copy codec parameters to output stream");

    if (!(ofmt->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ofmt->pb, str(path).c_str(), AVIO_FLAG_WRITE);
        av::Assert(ret, path, "Failed to open output I/O");
    }

    ret = avformat_write_header(ofmt.get(), nullptr);
    av::Assert(ret, path, "Failed to write output format header");

    return ost;
}

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
AVFilterContext *Filter(const AVFilterGraphPtr &graph, AVFilterContext *from, const char *name,
                               const char *instance, fmt::format_string<Args...> fmt, Args &&... args) {
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
