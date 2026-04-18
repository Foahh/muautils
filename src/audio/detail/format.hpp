#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include "audio/detail/raii.hpp"
#include "audio/detail/target_format.hpp"
#include "lib.hpp"

namespace Audio::detail {

AVFormatInputContextPtr OpenAVFormatInput(const fs::path &path);
AVFormatOutputContextPtr OpenAVFormatOutput(const fs::path &path);

AVStream *GetBestAudioStream(const AVFormatInputContextPtr &ctx);

AVCodecContextPtr OpenDecoder(const AVStream *st);
AVCodecContextPtr OpenEncoder(const TargetFormat &params);

AVStream *OpenOutputStream(const fs::path &path,
                           const AVFormatOutputContextPtr &ofmt,
                           const AVCodecContextPtr &ectx);

} // namespace Audio::detail
