// src/audio/detail/analyze.hpp
#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/samplefmt.h>
}

#include "lib.hpp"

namespace Audio::detail {

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

} // namespace Audio::detail
