#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <libavutil/mem.h>
}

#include <memory>

namespace Audio::detail {

struct AVCharDeleter {
    void operator()(char *ptr) const {
        av_freep(&ptr);
    }
};
using AVCharPtr = std::unique_ptr<char, AVCharDeleter>;

struct AVFormatInputContextDeleter {
    void operator()(AVFormatContext *ctx) const {
        avformat_close_input(&ctx);
    }
};
using AVFormatInputContextPtr = std::unique_ptr<AVFormatContext, AVFormatInputContextDeleter>;

struct AVFormatOutputContextDeleter {
    void operator()(AVFormatContext *ctx) const {
        if (!ctx)
            return;
        if (ctx->pb)
            avio_closep(&ctx->pb);
        avformat_free_context(ctx);
    }
};
using AVFormatOutputContextPtr = std::unique_ptr<AVFormatContext, AVFormatOutputContextDeleter>;

struct AVCodecContextDeleter {
    void operator()(AVCodecContext *ctx) const {
        avcodec_free_context(&ctx);
    }
};
using AVCodecContextPtr = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>;

struct AVFilterGraphDeleter {
    void operator()(AVFilterGraph *graph) const {
        avfilter_graph_free(&graph);
    }
};
using AVFilterGraphPtr = std::unique_ptr<AVFilterGraph, AVFilterGraphDeleter>;

struct AVPacketDeleter {
    void operator()(AVPacket *pkt) const {
        av_packet_free(&pkt);
    }
};
using AVPacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;

struct AVFrameDeleter {
    void operator()(AVFrame *frame) const {
        av_frame_free(&frame);
    }
};
using AVFramePtr = std::unique_ptr<AVFrame, AVFrameDeleter>;

} // namespace Audio::detail
