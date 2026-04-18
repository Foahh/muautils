#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
}

#include <fmt/format.h>
#include <memory>

#include "audio.hpp"
#include "audio/detail/error.hpp"
#include "audio/detail/filter.hpp"
#include "audio/detail/format.hpp"
#include "audio/detail/pipeline.hpp"
#include "audio/detail/raii.hpp"
#include "lib.hpp"
#include "utils.hpp"

namespace Audio {
using detail::AVCharPtr;
using detail::AVFormatInputContextPtr;
using detail::AVFormatOutputContextPtr;
using detail::AVCodecContextPtr;
using detail::AVFilterGraphPtr;
using detail::AVPacketPtr;
using detail::AVFramePtr;

using detail::OpenAVFormatInput;
using detail::OpenAVFormatOutput;
using detail::GetBestAudioStream;
using detail::OpenDecoder;
using detail::OpenEncoder;
using detail::OpenOutputStream;

using detail::Filter;
using detail::BufferSource;

using detail::RunGraph;
using detail::Encode;
}
