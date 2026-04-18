// src/audio/detail/pipeline.cpp
#include "audio/detail/pipeline.hpp"

extern "C" {
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
}

#include "audio/detail/error.hpp"

namespace Audio::detail {

namespace {

void pumpDecoder(const AVCodecContextPtr &decoder,
                 AVFilterContext *src,
                 AVFilterContext *sink,
                 const AVFramePtr &dfrm,
                 const AVFramePtr &ffrm,
                 const OnFrame &onFrame) {
    for (;;) {
        auto ret = avcodec_receive_frame(decoder.get(), dfrm.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
        av::Check(ret, "Failed to receive frame from decoder");

        ret = av_buffersrc_add_frame(src, dfrm.get());
        av::Check(ret, "Failed to add frame to buffer source: {}", src->filter->name);
        av_frame_unref(dfrm.get());

        while (av_buffersink_get_frame(sink, ffrm.get()) == 0) {
            onFrame(ffrm.get());
            av_frame_unref(ffrm.get());
        }
    }
}

} // namespace

void RunGraph(const AVFormatInputContextPtr &input,
              const AVStream *stream,
              const AVCodecContextPtr &decoder,
              AVFilterContext *src,
              AVFilterContext *sink,
              const OnFrame &onFrame) {
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
        pumpDecoder(decoder, src, sink, dfrm, ffrm, onFrame);
    }

    auto ret = avcodec_send_packet(decoder.get(), nullptr);
    av::Check(ret, "Failed to send end-of-stream packet to decoder: {}", decoder->codec->name);
    pumpDecoder(decoder, src, sink, dfrm, ffrm, onFrame);

    ret = av_buffersrc_add_frame(src, nullptr);
    av::Check(ret, "Failed to add end-of-stream frame to buffer source: {}", src->filter->name);

    while (av_buffersink_get_frame(sink, ffrm.get()) == 0) {
        onFrame(ffrm.get());
        av_frame_unref(ffrm.get());
    }
}

void Encode(const AVFramePtr &frame,
            const AVCodecContextPtr &encoder,
            const AVFormatOutputContextPtr &output,
            const AVPacketPtr &pkt,
            const AVStream *srcStream) {
    if (frame->pts != AV_NOPTS_VALUE) {
        frame->pts = av_rescale_q(frame->pts, srcStream->time_base, encoder->time_base);
    }

    auto ret = avcodec_send_frame(encoder.get(), frame.get());
    av::Check(ret, "Failed to send frame to encoder: {}", encoder->codec->name);

    while (avcodec_receive_packet(encoder.get(), pkt.get()) == 0) {
        ret = av_interleaved_write_frame(output.get(), pkt.get());
        av::Check(ret, "Failed to write packet to output format: {}", output->oformat->name);
        av_packet_unref(pkt.get());
    }
}

} // namespace Audio::detail
