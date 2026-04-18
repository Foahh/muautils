extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/log.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
}

#include "audio.hpp"
#include "lib.hpp"
#include "audio/detail/error.hpp"
#include "audio/detail/filter.hpp"
#include "audio/detail/format.hpp"
#include "audio/detail/pipeline.hpp"
#include "audio/detail/raii.hpp"

using namespace Audio::detail;

#include <spdlog/spdlog.h>

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
    av::Assert(ret, "No audio stream found in file");
}

Audio::AudioStreamMeta Audio::Analyze(const fs::path &path) {
    const auto ifmt = OpenAVFormatInput(path);
    const auto ist = GetBestAudioStream(ifmt);
    const auto dctx = OpenDecoder(ist);

    AudioStreamMeta meta{};
    meta.StreamIndex = ist->index;
    meta.MediaType = dctx->codec_type;
    meta.CodecId = dctx->codec_id;
    meta.SampleFormat = dctx->sample_fmt;
    meta.SampleRate = dctx->sample_rate;
    meta.Channels = dctx->ch_layout.nb_channels;

    if (av_sample_fmt_is_planar(dctx->sample_fmt) == 0 || dctx->sample_fmt != AV_SAMPLE_FMT_FLTP) {
        dctx->request_sample_fmt = AV_SAMPLE_FMT_FLTP;
    }

    const AVFilterGraphPtr graph(avfilter_graph_alloc());
    av::Ensure(graph.get(), "Failed to allocate filter graph");

    AVFilterContext *fsrc = BufferSource(graph, dctx);
    AVFilterContext *ebur = Filter(graph, fsrc, "ebur128", "ebur128", "peak=true:framelog=quiet");
    AVFilterContext *fsnk = Filter(graph, ebur, "abuffersink", "out");

    auto ret = avfilter_graph_config(graph.get(), nullptr);
    av::Assert(ret, "Failed to configure filter graph for audio analysis");

    RunGraph(ifmt, ist, dctx, fsrc, fsnk, [](AVFrame *) { /* no-op: ebur128 accumulates internally */ });

    ret = av_opt_get_double(ebur->priv, "integrated", 0, &meta.Loudness);
    av::Assert(ret, "Failed to get integrated loudness from ebur128 filter: {}", ebur->filter->name);

    ret = av_opt_get_double(ebur->priv, "true_peak", 0, &meta.TruePeak);
    av::Assert(ret, "Failed to get true peak from ebur128 filter: {}", ebur->filter->name);

    return meta;
}