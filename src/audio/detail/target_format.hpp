// src/audio/detail/target_format.hpp
#pragma once

extern "C" {
#include <libavcodec/codec_id.h>
#include <libavutil/samplefmt.h>
}

namespace Audio::detail {

struct TargetFormat {
    AVCodecID CodecId;

    AVSampleFormat SampleFormat;
    int SampleRate;

    double Loudness; // LUFS
    double Limit;    // dB
    int Attack;      // ms
    int Release;     // ms

    double TruePeakTolerance; // dB
    double GainTolerance;     // dB
    double OffsetTolerance;   // seconds
};

inline constexpr TargetFormat kTargetFormat = {
    AV_CODEC_ID_PCM_S16LE, AV_SAMPLE_FMT_S16, 48000, -8.0, 0, 12, 200, 1, 1, 0.0001};

} // namespace Audio::detail
