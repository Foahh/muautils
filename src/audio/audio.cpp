extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/log.h>
#include <libavutil/samplefmt.h>
}

#include <cmath>
#include <cstdint>

#include "audio.hpp"
#include "audio/detail/error.hpp"
#include "audio/detail/filter.hpp"
#include "audio/detail/format.hpp"
#include "audio/detail/loudnorm.hpp"
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
    bool needLoudNorm;
    bool needOffset;
    LoudNormStats loudNorm;

    bool isNoop() const {
        return !needTransform && !needFormat && !needChannels && !needLoudNorm && !needOffset;
    }
};

NormalizePlan planNormalize(const AVCodecContextPtr &dctx, const LoudNormStats &stats, const double offset) {
    using Audio::detail::DefaultTarget;
    NormalizePlan p{};
    p.loudNorm = stats;
    p.needTransform = dctx->codec_id != DefaultTarget.CodecId;
    p.needFormat = dctx->sample_rate != DefaultTarget.SampleRate || dctx->sample_fmt != DefaultTarget.SampleFormat;
    p.needChannels = dctx->ch_layout.nb_channels != 2;
    p.needLoudNorm = std::abs(stats.InputI - DefaultTarget.Loudness) >= DefaultTarget.GainTolerance ||
                     stats.InputTP > DefaultTarget.Limit + DefaultTarget.TruePeakTolerance ||
                     stats.InputLRA > DefaultTarget.LoudnessRange + DefaultTarget.LoudnessRangeTolerance;
    p.needOffset = std::abs(offset) >= DefaultTarget.OffsetTolerance;
    return p;
}

struct NormalizeChain {
    AVFilterContext *src;
    AVFilterContext *sink;
};

NormalizeChain buildChain(const Audio::detail::AVFilterGraphPtr &graph, const Audio::detail::AVCodecContextPtr &dctx,
                          const NormalizePlan &plan, double offset) {
    using namespace Audio::detail;

    AVFilterContext *fsrc = BufferSource(graph, dctx);
    AVFilterContext *flast = ApplyOffset(graph, dctx, fsrc, offset);

    if (plan.needLoudNorm) {
        spdlog::info("Applying two-pass loudnorm filter");
        flast = ApplyLoudNorm(graph, flast, plan.loudNorm);
    }

    flast = Filter(graph, flast, "aformat", "aformat", "sample_fmts={}:sample_rates={}:channel_layouts=stereo",
                   av_get_sample_fmt_name(DefaultTarget.SampleFormat), DefaultTarget.SampleRate);

    AVFilterContext *fsnk = Filter(graph, flast, "abuffersink", "abuffersink");
    return {fsrc, fsnk};
}

void seekInputToStart(const Audio::detail::AVFormatInputContextPtr &ifmt, const AVStream *ist,
                      const Audio::detail::AVCodecContextPtr &dctx, const fs::path &src) {
    const auto ret = avformat_seek_file(ifmt.get(), ist->index, INT64_MIN, 0, INT64_MAX, 0);
    av::Check(ret, src, "Failed to seek input to start after loudness analysis");
    avcodec_flush_buffers(dctx.get());
}

} // namespace

bool Audio::Normalize(const fs::path &src, const fs::path &dst, const double offset) {
    using namespace Audio::detail;

    const auto ifmt = OpenAVFormatInput(src);
    const auto ist = GetBestAudioStream(ifmt);
    const auto dctx = OpenDecoder(ist);
    const auto loudNormStats = AnalyzeLoudNorm(ifmt, ist, dctx, offset);

    const auto plan = planNormalize(dctx, loudNormStats, offset);
    if (plan.isNoop())
        return false;

    seekInputToStart(ifmt, ist, dctx, src);

    const auto ofmt = OpenAVFormatOutput(dst);
    const AVCodecContextPtr ectx = OpenEncoder(DefaultTarget);
    AVStream *const ost = OpenOutputStream(dst, ofmt, ectx);

    const AVFilterGraphPtr graph(avfilter_graph_alloc());
    av::Require(graph.get(), "Failed to allocate filter graph");

    const auto chain = buildChain(graph, dctx, plan, offset);

    auto ret = avfilter_graph_config(graph.get(), nullptr);
    av::Check(ret, "Failed to configure filter graph.");

    av::Require(chain.src && chain.sink, "Failed to build normalize filter chain");

    const AVRational sinkTb = av_buffersink_get_time_base(chain.sink);

    const AVPacketPtr pkt(av_packet_alloc());
    av::Require(pkt.get(), "Failed to allocate packet");

    RunGraph(ifmt, ist, dctx, chain.src, chain.sink, [&](AVFrame *f) {
        AVFramePtr owned(av_frame_alloc());
        av::Require(owned.get(), "Failed to allocate frame");
        av_frame_move_ref(owned.get(), f);
        Encode(owned, ectx, ofmt, ost, pkt, sinkTb);
    });

    FlushEncoder(ectx, ofmt, ost, pkt);

    ret = av_write_trailer(ofmt.get());
    av::Check(ret, "Failed to write trailer to output format: {}", ofmt->oformat->name);

    return true;
}
