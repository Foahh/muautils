#include <spdlog/spdlog.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
}

#include "audio.hpp"
#include "lib.hpp"
#include "utils.hpp"

namespace Audio {

bool Normalize(const fs::path &srcPath, const fs::path &dstPath, const double offset, const NormalizeFormat &target) {
    const auto meta = Analyze(srcPath);
    const double gain = target.Loudness - meta.Loudness;

    const auto ifmt = OpenAVFormatInput(srcPath);
    const auto ist = GetBestAudioStream(ifmt);
    const auto dctx = OpenDecoder(ist);

    const bool needTransform = dctx->codec_id != target.CodecId;
    const bool needFormat = meta.SampleRate != target.SampleRate || meta.Channels != target.Channels ||
                            meta.SampleFormat != target.SampleFormat;
    const bool needVolume = std::abs(gain) >= target.GainTolerance;
    const bool needLimit = std::abs(meta.TruePeak - target.Limit) >= target.TruePeakTolerance;
    const bool needOffset = std::abs(offset) >= target.OffsetTolerance;

    if (!needTransform && !needFormat && !needVolume && !needLimit && !needOffset) {
        return false;
    }

    const auto ofmt = OpenAVFormatOutput(dstPath);
    const AVCodecContextPtr ectx = OpenEncoder(FMT_PCM_S16LE_8LU);
    OpenOutputStream(dstPath, ofmt, ectx);

    const AVFilterGraphPtr graph(avfilter_graph_alloc());
    av::Ensure(graph.get(), "Failed to allocate filter graph");

    AVFilterContext *fsrc = BufferSource(graph, dctx);
    AVFilterContext *flast = fsrc;

    if (needOffset) {
        spdlog::info("Applying offset filter");
        if (offset > 0.0) {
            flast = Filter(graph, flast, "adelay", "adelay", "delays={}s:all=1", offset);
        } else {
            flast = Filter(graph, flast, "atrim", "atrim", "start={}s", -offset);
        }
    }

    if (needVolume) {
        spdlog::info("Applying volume filter");
        flast = Filter(graph, flast, "volume", "volume", "volume={}dB", gain);
    }

    if (needLimit) {
        spdlog::info("Applying limiter filter");
        flast = Filter(graph, flast, "alimiter", "alimiter",
                             "limit={}dB:attack={}:release={}:level=0", FMT_PCM_S16LE_8LU.Limit,
                             FMT_PCM_S16LE_8LU.Attack, FMT_PCM_S16LE_8LU.Release);
    }

    // TODO: channel_layouts=stereo
    if (needFormat) {
        spdlog::info("Applying format conversion filter");
        flast = Filter(graph, flast, "aformat", "aformat",
                             "sample_fmts={}:sample_rates={}:channel_layouts=stereo",
                             av_get_sample_fmt_name(FMT_PCM_S16LE_8LU.SampleFormat),
                             FMT_PCM_S16LE_8LU.SampleRate);
    }

    AVFilterContext *fsnk = Filter(graph, flast, "abuffersink", "abuffersink");

    auto ret = avfilter_graph_config(graph.get(), nullptr);
    av::Assert(ret, "Failed to configure filter graph.");

    const AVPacketPtr pkt(av_packet_alloc());
    const AVFramePtr dfrm(av_frame_alloc());
    const AVFramePtr ffrm(av_frame_alloc());
    av::Ensure(pkt && dfrm && ffrm, "Failed to allocate packet or frame");

    while (av_read_frame(ifmt.get(), pkt.get()) >= 0) {
        if (pkt->stream_index != ist->index) {
            av_packet_unref(pkt.get());
            continue;
        }

        ret = avcodec_send_packet(dctx.get(), pkt.get());
        av::Assert(ret, "Failed to send packet to decoder: {}", dctx->codec->name);

        av_packet_unref(pkt.get());
        DecodeEncode(fsrc, fsnk, dctx, ist, ectx, ofmt, pkt, dfrm, ffrm);
    }

    ret = avcodec_send_packet(dctx.get(), nullptr);
    av::Assert(ret, "Failed to send end-of-stream packet to decoder: {} ", dctx->codec->name);
    DecodeEncode(fsrc, fsnk, dctx, ist, ectx, ofmt, pkt, dfrm, ffrm);

    ret = av_buffersrc_add_frame(fsrc, nullptr);
    av::Assert(ret, "Failed to add end-of-stream frame to buffer source: {}", fsrc->filter->name);
    while (av_buffersink_get_frame(fsnk, ffrm.get()) == 0) {
        Encode(ffrm, ectx, ofmt, pkt, ist);
        av_frame_unref(ffrm.get());
    }

    ret = avcodec_send_frame(ectx.get(), nullptr);
    av::Assert(ret, "Failed to send end-of-stream frame to encoder: {}", ectx->codec->name);

    while (avcodec_receive_packet(ectx.get(), pkt.get()) == 0) {
        ret = av_interleaved_write_frame(ofmt.get(), pkt.get());
        av::Assert(ret, "Failed to write packet to output format: {}", ofmt->oformat->name);
        av_packet_unref(pkt.get());
    }

    ret = av_write_trailer(ofmt.get());
    av::Assert(ret, "Failed to write trailer to output format: {}", ofmt->oformat->name);

    return true;
}


} // namespace Audio