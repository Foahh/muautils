// src/audio/detail/loudnorm.cpp
#include "audio/detail/loudnorm.hpp"

extern "C" {
#include <libavutil/opt.h>
}

#include "audio/detail/error.hpp"
#include "audio/detail/filter.hpp"
#include "audio/detail/pipeline.hpp"
#include "audio/detail/target_format.hpp"
#include "lib.hpp"

#include <cerrno>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

#include <spdlog/spdlog.h>

namespace Audio::detail {

namespace {

class TempStatsFile {
  public:
    TempStatsFile() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        m_path = fs::temp_directory_path() /
                 fmt::format("muautils-loudnorm-{}-{}.json", stamp, reinterpret_cast<std::uintptr_t>(this));
    }

    ~TempStatsFile() {
        std::error_code ec;
        fs::remove(m_path, ec);
    }

    [[nodiscard]] const fs::path &path() const {
        return m_path;
    }

  private:
    fs::path m_path;
};

std::string ReadFile(const fs::path &path) {
    std::ifstream in(path, std::ios::binary);
    av::Require(in.good(), "Failed to open loudnorm stats file: {}", lib::PathToUtf8(path));
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

double ReadJsonDouble(const std::string &json, const char *key) {
    const auto needle = fmt::format("\"{}\"", key);
    auto pos = json.find(needle);
    av::Require(pos != std::string::npos, "Missing loudnorm stat: {}", key);

    pos = json.find(':', pos + needle.size());
    av::Require(pos != std::string::npos, "Malformed loudnorm stat: {}", key);
    ++pos;

    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }
    if (pos < json.size() && json[pos] == '"') {
        ++pos;
    }

    errno = 0;
    char *end = nullptr;
    const double value = std::strtod(json.c_str() + pos, &end);
    av::Require(end != json.c_str() + pos && errno != ERANGE, "Invalid loudnorm stat: {}", key);
    return value;
}

LoudNormStats ReadLoudNormStats(const fs::path &path) {
    const auto json = ReadFile(path);
    return {
        ReadJsonDouble(json, "input_i"),
        ReadJsonDouble(json, "input_tp"),
        ReadJsonDouble(json, "input_lra"),
        ReadJsonDouble(json, "input_thresh"),
        ReadJsonDouble(json, "target_offset"),
    };
}

void SetDoubleOption(AVFilterContext *ctx, const char *name, const double value) {
    const auto ret = av_opt_set_double(ctx->priv, name, value, 0);
    av::Check(ret, "Failed to set {} option on filter: {}", name, ctx->filter->name);
}

void SetIntOption(AVFilterContext *ctx, const char *name, const int64_t value) {
    const auto ret = av_opt_set_int(ctx->priv, name, value, 0);
    av::Check(ret, "Failed to set {} option on filter: {}", name, ctx->filter->name);
}

void SetStringOption(AVFilterContext *ctx, const char *name, const std::string &value) {
    const auto ret = av_opt_set(ctx->priv, name, value.c_str(), 0);
    av::Check(ret, "Failed to set {} option on filter: {}", name, ctx->filter->name);
}

AVFilterContext *AddLoudNorm(const AVFilterGraphPtr &graph, AVFilterContext *from, const char *instance,
                             const LoudNormStats *stats, const fs::path *statsFile) {
    AVFilterContext *ctx = Filter(graph, "loudnorm", instance);
    SetDoubleOption(ctx, "I", DefaultTarget.Loudness);
    SetDoubleOption(ctx, "LRA", DefaultTarget.LoudnessRange);
    SetDoubleOption(ctx, "TP", DefaultTarget.Limit);
    SetIntOption(ctx, "linear", 1);

    if (stats) {
        SetDoubleOption(ctx, "measured_I", stats->InputI);
        SetDoubleOption(ctx, "measured_TP", stats->InputTP);
        SetDoubleOption(ctx, "measured_LRA", stats->InputLRA);
        SetDoubleOption(ctx, "measured_thresh", stats->InputThresh);
        SetDoubleOption(ctx, "offset", stats->TargetOffset);
    }

    if (statsFile) {
        SetStringOption(ctx, "print_format", "json");
        SetStringOption(ctx, "stats_file", lib::PathToUtf8(*statsFile));
    }

    auto ret = avfilter_init_str(ctx, nullptr);
    av::Check(ret, "Failed to initialize filter: {}", ctx->filter->name);
    ret = avfilter_link(from, 0, ctx, 0);
    av::Check(ret, "Failed to link filter: {}", ctx->filter->name);
    return ctx;
}

} // namespace

AVFilterContext *ApplyOffset(const AVFilterGraphPtr &graph, const AVCodecContextPtr &dctx, AVFilterContext *from,
                             const double offset) {
    if (std::abs(offset) < DefaultTarget.OffsetTolerance) {
        return from;
    }

    spdlog::info("Applying offset filter");
    if (offset > 0.0) {
        const int64_t delayMs = llround(offset * 1000.0);
        return Filter(graph, from, "adelay", "adelay", "delays={}:all=1", delayMs);
    }

    const double cutSeconds = -offset;
    const int64_t startSample = llround(cutSeconds * dctx->sample_rate);
    from = Filter(graph, from, "atrim", "atrim", "start_sample={}", startSample);
    return Filter(graph, from, "asetpts", "asetpts", "expr=PTS-STARTPTS");
}

AVFilterContext *ApplyLoudNorm(const AVFilterGraphPtr &graph, AVFilterContext *from, const LoudNormStats &stats) {
    return AddLoudNorm(graph, from, "loudnorm", &stats, nullptr);
}

LoudNormStats AnalyzeLoudNorm(const AVFormatInputContextPtr &ifmt, const AVStream *ist, const AVCodecContextPtr &dctx,
                              const double offset) {
    TempStatsFile statsFile;

    {
        const AVFilterGraphPtr graph(avfilter_graph_alloc());
        av::Require(graph.get(), "Failed to allocate filter graph");

        AVFilterContext *fsrc = BufferSource(graph, dctx);
        AVFilterContext *flast = ApplyOffset(graph, dctx, fsrc, offset);
        flast = AddLoudNorm(graph, flast, "loudnorm_probe", nullptr, &statsFile.path());
        AVFilterContext *fsnk = Filter(graph, flast, "abuffersink", "loudnorm_probe_out");

        const auto ret = avfilter_graph_config(graph.get(), nullptr);
        av::Check(ret, "Failed to configure loudnorm analysis filter graph");

        RunGraph(ifmt, ist, dctx, fsrc, fsnk, [](AVFrame *) { /* stats are written when the graph closes */ });
    }

    const auto stats = ReadLoudNormStats(statsFile.path());
    spdlog::info("Loudnorm first pass: I={} TP={} LRA={} threshold={} target_offset={}", stats.InputI, stats.InputTP,
                 stats.InputLRA, stats.InputThresh, stats.TargetOffset);
    return stats;
}

} // namespace Audio::detail
