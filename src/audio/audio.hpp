#pragma once

#include "lib.hpp"

extern "C" {
#include <libavutil/samplefmt.h>
}

namespace Audio {

struct NormalizeOptions {
    double Offset = 0.0;

    AVSampleFormat SampleFormat = AV_SAMPLE_FMT_S16;
    int SampleRate = 48000;

    double Loudness = -8.25;     // LUFS
    double LoudnessRange = 11.0; // LU
    double TruePeak = 0.0;       // dBTP

    double TruePeakTolerance = 0.5;      // dB
    double LoudnessRangeTolerance = 0.1; // LU
    double GainTolerance = 0.2;          // dB
    double OffsetTolerance = 0.0001;     // seconds
};

void Initialize();

void EnsureValid(const fs::path &path);

bool Normalize(const fs::path &src, const fs::path &dst, const NormalizeOptions &options);

} // namespace Audio
