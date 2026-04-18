// src/audio/detail/filter.cpp
#include "audio/detail/filter.hpp"

extern "C" {
#include <libavutil/channel_layout.h>
}

namespace Audio::detail {

AVFilterContext *Filter(const AVFilterGraphPtr &graph, const char *name, const char *instance) {
    const AVFilter *filt = avfilter_get_by_name(name);
    av::Require(filt, "Filter not found: {}", name);
    AVFilterContext *ctx = avfilter_graph_alloc_filter(graph.get(), filt, instance);
    av::Require(ctx, "Failed to allocate filter context");
    return ctx;
}

AVFilterContext *Filter(const AVFilterGraphPtr &graph, AVFilterContext *from,
                        const char *name, const char *instance, const char *opts) {
    AVFilterContext *ctx = Filter(graph, name, instance);

    auto ret = avfilter_init_str(ctx, opts);
    av::Check(ret, "Failed to initialize filter: {}", name);

    ret = avfilter_link(from, 0, ctx, 0);
    av::Check(ret, "Failed to link filter: {}", name);

    return ctx;
}

AVFilterContext *BufferSource(const AVFilterGraphPtr &graph, const AVCodecContextPtr &codec) {
    AVFilterContext *src = Filter(graph, "abuffer", "in");
    AVBufferSrcParameters *par = av_buffersrc_parameters_alloc();
    av::Require(par, "Failed to allocate buffer source parameters");

    par->format = codec->sample_fmt;
    par->sample_rate = codec->sample_rate;
    av_channel_layout_copy(&par->ch_layout, &codec->ch_layout);
    par->time_base = codec->time_base;

    auto ret = av_buffersrc_parameters_set(src, par);
    av::Check(ret, "Failed to set parameters for buffer source: {}", src->filter->name);

    ret = avfilter_init_str(src, nullptr);
    av::Check(ret, "Failed to initialize buffer source: {}", src->filter->name);

    av_freep(&par);
    return src;
}

} // namespace Audio::detail
