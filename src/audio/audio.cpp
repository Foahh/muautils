extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/log.h>
#include <libavutil/samplefmt.h>
}

#include "audio.hpp"
#include "lib.hpp"
#include "audio/detail/analyze.hpp"
#include "audio/detail/error.hpp"
#include "audio/detail/filter.hpp"
#include "audio/detail/format.hpp"
#include "audio/detail/pipeline.hpp"
#include "audio/detail/raii.hpp"
#include "audio/detail/target_format.hpp"

#include <spdlog/spdlog.h>

using namespace Audio::detail;

inline int spdlog_to_av_level(const spdlog::level::level_enum lvl) {
    switch (lvl) {
    case spdlog::level::trace:
    case spdlog::level::debug:
        return AV_LOG_DEBUG;
    case spdlog::level::info:
        return AV_LOG_INFO;
    case spdlog::level::warn:
        return AV_LOG_WARNING;
    case spdlog::level::err:
        return AV_LOG_ERROR;
    case spdlog::level::critical:
        return AV_LOG_FATAL;
    case spdlog::level::off:
        return AV_LOG_QUIET;
    default:
        return AV_LOG_INFO;
    }
}

void Audio::Initialize() {
    av_log_set_level(spdlog_to_av_level(spdlog::get_level()));
}

void Audio::EnsureValid(const fs::path &path) {
    const auto fmt = OpenAVFormatInput(path);
    const auto ret = av_find_best_stream(fmt.get(), AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    av::Check(ret, "No audio stream found in file");
}

namespace {

struct NormalizePlan {
    bool needTransform;
    bool needFormat;
    bool needChannels;
    bool needVolume;
    bool needLimit;
    bool needOffset;
    double gain;

    bool isNoop() const {
        return !needTransform && !needFormat && !needChannels && !needVolume && !needLimit && !needOffset;
    }
};

NormalizePlan planNormalize(const Audio::detail::AudioStreamMeta &meta,
                            AVCodecID srcCodecId,
                            double offset) {
    using Audio::detail::DefaultTarget;
    NormalizePlan p{};
    p.gain = DefaultTarget.Loudness - meta.Loudness;
    p.needTransform = srcCodecId != DefaultTarget.CodecId;
    p.needFormat = meta.SampleRate != DefaultTarget.SampleRate ||
                   meta.SampleFormat != DefaultTarget.SampleFormat;
    p.needChannels = meta.Channels != 2;
    p.needVolume = std::abs(p.gain) >= DefaultTarget.GainTolerance;
    p.needLimit = std::abs(meta.TruePeak - DefaultTarget.Limit) >= DefaultTarget.TruePeakTolerance;
    p.needOffset = std::abs(offset) >= DefaultTarget.OffsetTolerance;
    return p;
}

AVFilterContext *buildChain(const Audio::detail::AVFilterGraphPtr &graph,
                            const Audio::detail::AVCodecContextPtr &dctx,
                            const NormalizePlan &plan,
                            double offset) {
    using namespace Audio::detail;

    AVFilterContext *fsrc = BufferSource(graph, dctx);
    AVFilterContext *flast = fsrc;

    if (plan.needOffset) {
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

    if (plan.needVolume) {
        spdlog::info("Applying volume filter");
        flast = Filter(graph, flast, "volume", "volume", "volume={}dB", plan.gain);
    }

    if (plan.needLimit) {
        spdlog::info("Applying limiter filter");
        flast = Filter(graph, flast, "alimiter", "alimiter", "limit={}dB:attack={}:release={}:level=0",
                       DefaultTarget.Limit, DefaultTarget.Attack, DefaultTarget.Release);
    }

    flast = Filter(graph, flast, "aformat", "aformat",
                   "sample_fmts={}:sample_rates={}:channel_layouts=stereo",
                   av_get_sample_fmt_name(DefaultTarget.SampleFormat), DefaultTarget.SampleRate);

    return Filter(graph, flast, "abuffersink", "abuffersink");
}

} // namespace

bool Audio::Normalize(const fs::path &src, const fs::path &dst, const double offset) {
    using namespace Audio::detail;

    const auto meta = Analyze(src);

    const auto ifmt = OpenAVFormatInput(src);
    const auto ist = GetBestAudioStream(ifmt);
    const auto dctx = OpenDecoder(ist);

    const auto plan = planNormalize(meta, dctx->codec_id, offset);
    if (plan.isNoop()) return false;

    const auto ofmt = OpenAVFormatOutput(dst);
    const AVCodecContextPtr ectx = OpenEncoder(DefaultTarget);
    OpenOutputStream(dst, ofmt, ectx);

    const AVFilterGraphPtr graph(avfilter_graph_alloc());
    av::Require(graph.get(), "Failed to allocate filter graph");

    AVFilterContext *fsnk = buildChain(graph, dctx, plan, offset);

    auto ret = avfilter_graph_config(graph.get(), nullptr);
    av::Check(ret, "Failed to configure filter graph.");

    AVFilterContext *fsrc = nullptr;
    for (unsigned i = 0; i < graph->nb_filters; ++i) {
        if (std::string(graph->filters[i]->filter->name) == "abuffer") {
            fsrc = graph->filters[i];
            break;
        }
    }
    av::Require(fsrc, "Could not locate buffer source in graph");

    const AVPacketPtr pkt(av_packet_alloc());
    av::Require(pkt.get(), "Failed to allocate packet");

    RunGraph(ifmt, ist, dctx, fsrc, fsnk,
             [&](AVFrame *f) {
                 AVFramePtr owned(av_frame_alloc());
                 av::Require(owned.get(), "Failed to allocate frame");
                 av_frame_move_ref(owned.get(), f);
                 Encode(owned, ectx, ofmt, pkt, ist);
             });

    ret = avcodec_send_frame(ectx.get(), nullptr);
    av::Check(ret, "Failed to send end-of-stream frame to encoder: {}", ectx->codec->name);

    while (avcodec_receive_packet(ectx.get(), pkt.get()) == 0) {
        ret = av_interleaved_write_frame(ofmt.get(), pkt.get());
        av::Check(ret, "Failed to write packet to output format: {}", ofmt->oformat->name);
        av_packet_unref(pkt.get());
    }

    ret = av_write_trailer(ofmt.get());
    av::Check(ret, "Failed to write trailer to output format: {}", ofmt->oformat->name);

    return true;
}
