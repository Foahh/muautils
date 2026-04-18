#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
}

#include "lib.hpp"
#include <spdlog/common.h>

namespace Audio {

void Initialize();

struct AudioStreamMeta {
    int StreamIndex = -1;
    AVMediaType MediaType = AVMEDIA_TYPE_UNKNOWN;
    AVCodecID CodecId = AV_CODEC_ID_NONE;
    AVSampleFormat SampleFormat = AV_SAMPLE_FMT_NONE;
    int SampleRate = 0;
    int Channels = 0;
    double Loudness = 0.0;
    double TruePeak = 0.0;
};

AudioStreamMeta Analyze(const fs::path &path);

void EnsureValid(const fs::path &path);

} // namespace Audio