#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include "audio/audio.hpp"
#include "audio/detail/raii.hpp"
#include "lib.hpp"

#include <string>

namespace Audio::detail {

std::string PathToUtf8(const fs::path &path);

AVFormatInputContextPtr OpenAVFormatInput(const fs::path &path);
AVFormatOutputContextPtr OpenAVFormatOutput(const fs::path &path);

AVStream *GetBestAudioStream(const AVFormatInputContextPtr &ctx);

AVCodecContextPtr OpenDecoder(const AVStream *st);
AVCodecContextPtr OpenEncoder(const ::Audio::NormalizeFormat &params);

AVStream *OpenOutputStream(const fs::path &path,
                           const AVFormatOutputContextPtr &ofmt,
                           const AVCodecContextPtr &ectx);

} // namespace Audio::detail
