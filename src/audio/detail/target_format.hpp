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

    double Loudness;      // LUFS
    double LoudnessRange; // LU
    double Limit;         // dBTP

    double TruePeakTolerance;     // dB
    double LoudnessRangeTolerance; // LU
    double GainTolerance;         // dB
    double OffsetTolerance;       // seconds
};

inline constexpr TargetFormat DefaultTarget = {
    AV_CODEC_ID_PCM_S16LE, AV_SAMPLE_FMT_S16, 48000, -8.25, 11.0, 0.0, 0.5, 0.1, 0.2, 0.0001};

} // namespace Audio::detail
