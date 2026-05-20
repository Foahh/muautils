// src/audio/detail/target_format.hpp
#pragma once

extern "C" {
#include <libavcodec/codec_id.h>
#include <libavutil/samplefmt.h>
}

#include "audio/audio.hpp"
#include "audio/detail/error.hpp"

namespace Audio::detail {

struct TargetFormat {
    AVCodecID CodecId;

    AVSampleFormat SampleFormat;
    int SampleRate;

    double Loudness;      // LUFS
    double LoudnessRange; // LU
    double TruePeak;      // dBTP

    double TruePeakTolerance;     // dB
    double LoudnessRangeTolerance; // LU
    double GainTolerance;         // dB
    double OffsetTolerance;       // seconds
};

inline AVCodecID CodecIdForSampleFormat(const AVSampleFormat sampleFormat) {
    switch (sampleFormat) {
    case AV_SAMPLE_FMT_U8:
        return AV_CODEC_ID_PCM_U8;
    case AV_SAMPLE_FMT_S16:
        return AV_CODEC_ID_PCM_S16LE;
    case AV_SAMPLE_FMT_S32:
        return AV_CODEC_ID_PCM_S32LE;
    case AV_SAMPLE_FMT_FLT:
        return AV_CODEC_ID_PCM_F32LE;
    case AV_SAMPLE_FMT_DBL:
        return AV_CODEC_ID_PCM_F64LE;
    case AV_SAMPLE_FMT_S64:
        return AV_CODEC_ID_PCM_S64LE;
    default:
        av::Require(false, "Unsupported audio sample format for WAV normalization output: {}",
                    av_get_sample_fmt_name(sampleFormat) ? av_get_sample_fmt_name(sampleFormat) : "unknown");
        return AV_CODEC_ID_NONE;
    }
}

inline TargetFormat MakeTargetFormat(const NormalizeOptions &options) {
    return {
        CodecIdForSampleFormat(options.SampleFormat),
        options.SampleFormat,
        options.SampleRate,
        options.Loudness,
        options.LoudnessRange,
        options.TruePeak,
        options.TruePeakTolerance,
        options.LoudnessRangeTolerance,
        options.GainTolerance,
        options.OffsetTolerance,
    };
}

} // namespace Audio::detail
