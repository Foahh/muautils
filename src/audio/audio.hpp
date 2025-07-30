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

struct NormalizeFormat {
    AVCodecID CodecId;

    AVSampleFormat SampleFormat;
    int SampleRate;
    int Channels;

    double Loudness; // LUFS
    double Limit; // dB
    int Attack; // ms
    int Release; // ms

    double TruePeakTolerance; // dB
    double GainTolerance; // dB
    double OffsetTolerance; // seconds
};

static constexpr NormalizeFormat FMT_PCM_S16LE_8LU = {
    AV_CODEC_ID_PCM_S16LE, AV_SAMPLE_FMT_S16, 48000, 2, -8.0, 0, 12, 200, 1, 1, 0.0001};

AudioStreamMeta Analyze(const fs::path &path);

bool Normalize(const fs::path &srcPath, const fs::path &dstPath, double offset = 0.0,
               const NormalizeFormat &target = FMT_PCM_S16LE_8LU);

void EnsureValid(const fs::path &path);

} // namespace Audio