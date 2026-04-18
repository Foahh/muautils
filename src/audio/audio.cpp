extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/log.h>
}

#include "audio.hpp"
#include "lib.hpp"
#include "audio/detail/error.hpp"
#include "audio/detail/format.hpp"

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
    av::Assert(ret, "No audio stream found in file");
}
