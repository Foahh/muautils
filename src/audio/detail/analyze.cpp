// src/audio/detail/analyze.cpp
#include "audio/detail/analyze.hpp"

extern "C" {
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
}

#include "audio/detail/error.hpp"
#include "audio/detail/filter.hpp"
#include "audio/detail/format.hpp"
#include "audio/detail/pipeline.hpp"

namespace Audio::detail {

AudioStreamMeta Analyze(const fs::path &path) {
    const auto ifmt = OpenAVFormatInput(path);
    const auto ist = GetBestAudioStream(ifmt);
    const auto dctx = OpenDecoder(ist);
    return Analyze(ifmt, ist, dctx);
}

AudioStreamMeta Analyze(const AVFormatInputContextPtr &ifmt, const AVStream *ist, const AVCodecContextPtr &dctx) {
    AudioStreamMeta meta{};
    meta.StreamIndex = ist->index;
    meta.MediaType = dctx->codec_type;
    meta.CodecId = dctx->codec_id;
    meta.SampleFormat = dctx->sample_fmt;
    meta.SampleRate = dctx->sample_rate;
    meta.Channels = dctx->ch_layout.nb_channels;

    const AVFilterGraphPtr graph(avfilter_graph_alloc());
    av::Require(graph.get(), "Failed to allocate filter graph");

    AVFilterContext *fsrc = BufferSource(graph, dctx);
    AVFilterContext *ebur = Filter(graph, fsrc, "ebur128", "ebur128", "peak=true:framelog=quiet");
    AVFilterContext *fsnk = Filter(graph, ebur, "abuffersink", "out");

    auto ret = avfilter_graph_config(graph.get(), nullptr);
    av::Check(ret, "Failed to configure filter graph for audio analysis");

    RunGraph(ifmt, ist, dctx, fsrc, fsnk, [](AVFrame *) { /* no-op: ebur128 accumulates internally */ });

    ret = av_opt_get_double(ebur->priv, "integrated", 0, &meta.Loudness);
    av::Check(ret, "Failed to get integrated loudness from ebur128 filter: {}", ebur->filter->name);

    ret = av_opt_get_double(ebur->priv, "true_peak", 0, &meta.TruePeak);
    av::Check(ret, "Failed to get true peak from ebur128 filter: {}", ebur->filter->name);

    return meta;
}

} // namespace Audio::detail
