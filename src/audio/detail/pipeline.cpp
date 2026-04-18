// src/audio/detail/pipeline.cpp
#include "audio/detail/pipeline.hpp"

extern "C" {
#include <libavutil/error.h>
}

namespace Audio::detail {

void WritePacket(const AVPacketPtr &pkt, const AVCodecContextPtr &encoder, const AVFormatOutputContextPtr &output,
                 const AVStream *ost) {
    pkt->stream_index = ost->index;
    av_packet_rescale_ts(pkt.get(), encoder->time_base, ost->time_base);
    const auto ret = av_interleaved_write_frame(output.get(), pkt.get());
    av::Check(ret, "Failed to write packet to output format: {}", output->oformat->name);
    av_packet_unref(pkt.get());
}

void DrainEncoder(const AVCodecContextPtr &encoder, const AVFormatOutputContextPtr &output, const AVStream *ost,
                  const AVPacketPtr &pkt) {
    for (;;) {
        const auto ret = avcodec_receive_packet(encoder.get(), pkt.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        av::Check(ret, "Failed to receive packet from encoder: {}", encoder->codec->name);
        WritePacket(pkt, encoder, output, ost);
    }
}

void Encode(const AVFramePtr &frame, const AVCodecContextPtr &encoder, const AVFormatOutputContextPtr &output,
            const AVStream *ost, const AVPacketPtr &pkt, const AVRational src_frame_time_base) {
    if (frame->pts != AV_NOPTS_VALUE && src_frame_time_base.num > 0 && src_frame_time_base.den > 0) {
        frame->pts = av_rescale_q(frame->pts, src_frame_time_base, encoder->time_base);
    }

    for (;;) {
        const auto ret = avcodec_send_frame(encoder.get(), frame.get());
        if (ret == AVERROR(EAGAIN)) {
            DrainEncoder(encoder, output, ost, pkt);
            continue;
        }
        av::Check(ret, "Failed to send frame to encoder: {}", encoder->codec->name);
        break;
    }

    DrainEncoder(encoder, output, ost, pkt);
}

void FlushEncoder(const AVCodecContextPtr &encoder, const AVFormatOutputContextPtr &output, const AVStream *ost,
                  const AVPacketPtr &pkt) {
    const auto ret = avcodec_send_frame(encoder.get(), nullptr);
    av::Check(ret, "Failed to send end-of-stream frame to encoder: {}", encoder->codec->name);

    for (;;) {
        const auto rret = avcodec_receive_packet(encoder.get(), pkt.get());
        if (rret == AVERROR_EOF)
            return;
        av::Check(rret, "Failed to receive packet while flushing encoder: {}", encoder->codec->name);
        WritePacket(pkt, encoder, output, ost);
    }
}

} // namespace Audio::detail
