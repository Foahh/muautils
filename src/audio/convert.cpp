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
#include "audio/detail/error.hpp"
#include "audio/detail/filter.hpp"
#include "audio/detail/format.hpp"
#include "audio/detail/pipeline.hpp"
#include "audio/detail/raii.hpp"

using namespace Audio::detail;

namespace Audio {

// Note: Only supports stereo output for now
bool Normalize(const fs::path &srcPath, const fs::path &dstPath, const double offset, const NormalizeFormat &target) {
    const auto meta = Analyze(srcPath);
    const double gain = target.Loudness - meta.Loudness;

    const auto ifmt = OpenAVFormatInput(srcPath);
    const auto ist = GetBestAudioStream(ifmt);
    const auto dctx = OpenDecoder(ist);

    const bool needTransform = dctx->codec_id != target.CodecId;
    const bool needFormat = meta.SampleRate != target.SampleRate || meta.SampleFormat != target.SampleFormat;
    const bool needChannels = meta.Channels != 2;
    const bool needVolume = std::abs(gain) >= target.GainTolerance;
    const bool needLimit = std::abs(meta.TruePeak - target.Limit) >= target.TruePeakTolerance;
    const bool needOffset = std::abs(offset) >= target.OffsetTolerance;

    if (!needTransform && !needFormat && !needChannels && !needVolume && !needLimit && !needOffset) {
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
            flast = Filter(graph, flast, "adelay", "adelay", "delays={}:all=1", offset * 1000);
        } else {
            const double cutSeconds = -offset;
            const int sr = dctx->sample_rate;
            const int64_t startSample = llround(cutSeconds * sr);

            flast = Filter(graph, flast, "atrim", "atrim", "start_sample={}", startSample);
            flast = Filter(graph, flast, "asetpts", "asetpts", "expr=PTS-STARTPTS");
        }
    }

    if (needVolume) {
        spdlog::info("Applying volume filter");
        flast = Filter(graph, flast, "volume", "volume", "volume={}dB", gain);
    }

    if (needLimit) {
        spdlog::info("Applying limiter filter");
        flast = Filter(graph, flast, "alimiter", "alimiter", "limit={}dB:attack={}:release={}:level=0",
                       FMT_PCM_S16LE_8LU.Limit, FMT_PCM_S16LE_8LU.Attack, FMT_PCM_S16LE_8LU.Release);
    }

    flast = Filter(graph, flast, "aformat", "aformat", "sample_fmts={}:sample_rates={}:channel_layouts=stereo",
                   av_get_sample_fmt_name(FMT_PCM_S16LE_8LU.SampleFormat), FMT_PCM_S16LE_8LU.SampleRate);

    AVFilterContext *fsnk = Filter(graph, flast, "abuffersink", "abuffersink");

    auto ret = avfilter_graph_config(graph.get(), nullptr);
    av::Assert(ret, "Failed to configure filter graph.");

    const AVPacketPtr pkt(av_packet_alloc());
    av::Ensure(pkt.get(), "Failed to allocate packet");

    RunGraph(ifmt, ist, dctx, fsrc, fsnk,
             [&](AVFrame *f) {
                 AVFramePtr owned(av_frame_alloc());
                 av::Ensure(owned.get(), "Failed to allocate frame");
                 av_frame_move_ref(owned.get(), f);
                 Encode(owned, ectx, ofmt, pkt, ist);
             });

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