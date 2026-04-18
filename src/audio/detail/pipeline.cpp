// src/audio/detail/pipeline.cpp
#include "audio/detail/pipeline.hpp"

extern "C" {
#include <libavutil/error.h>
}

namespace Audio::detail {

void Encode(const AVFramePtr &frame, const AVCodecContextPtr &encoder, const AVFormatOutputContextPtr &output,
            const AVPacketPtr &pkt, const AVRational src_frame_time_base) {
    if (frame->pts != AV_NOPTS_VALUE && src_frame_time_base.num > 0 && src_frame_time_base.den > 0) {
        frame->pts = av_rescale_q(frame->pts, src_frame_time_base, encoder->time_base);
    }

    for (;;) {
        auto ret = avcodec_send_frame(encoder.get(), frame.get());
        if (ret != AVERROR(EAGAIN)) {
            av::Check(ret, "Failed to send frame to encoder: {}", encoder->codec->name);
            break;
        }
        while (avcodec_receive_packet(encoder.get(), pkt.get()) == 0) {
            ret = av_interleaved_write_frame(output.get(), pkt.get());
            av::Check(ret, "Failed to write packet to output format: {}", output->oformat->name);
            av_packet_unref(pkt.get());
        }
    }

    while (avcodec_receive_packet(encoder.get(), pkt.get()) == 0) {
        auto ret = av_interleaved_write_frame(output.get(), pkt.get());
        av::Check(ret, "Failed to write packet to output format: {}", output->oformat->name);
        av_packet_unref(pkt.get());
    }
}

} // namespace Audio::detail
