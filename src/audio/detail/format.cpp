#include "audio/detail/format.hpp"

extern "C" {
#include <libavutil/channel_layout.h>
}

#include "audio/detail/error.hpp"

namespace Audio::detail {

std::string PathToUtf8(const fs::path &path) {
#ifdef _WIN32
    auto s = path.u8string();
    return {s.begin(), s.end()};
#else
    return path.string();
#endif
}

AVFormatInputContextPtr OpenAVFormatInput(const fs::path &path) {
    AVFormatContext *raw = nullptr;
    auto ret = avformat_open_input(&raw, PathToUtf8(path).c_str(), nullptr, nullptr);
    av::Assert(ret, path, "Failed to open input format context");
    auto ctx = AVFormatInputContextPtr(raw);
    ret = avformat_find_stream_info(ctx.get(), nullptr);
    av::Assert(ret, path, "Failed to find stream info");
    return ctx;
}

AVFormatOutputContextPtr OpenAVFormatOutput(const fs::path &path) {
    AVFormatContext *raw = nullptr;
    const auto ret = avformat_alloc_output_context2(&raw, nullptr, "wav", PathToUtf8(path).c_str());
    av::Assert(ret, path, "Failed to allocate output format context");
    return AVFormatOutputContextPtr(raw);
}

AVStream *GetBestAudioStream(const AVFormatInputContextPtr &ctx) {
    const int ret = av_find_best_stream(ctx.get(), AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    av::Assert(ret, "No audio stream found in input format context");
    return ctx->streams[ret];
}

AVCodecContextPtr OpenDecoder(const AVStream *st) {
    const AVCodec *codec = avcodec_find_decoder(st->codecpar->codec_id);
    av::Ensure(codec, "Failed to find decoder for stream codec");

    AVCodecContext *raw = avcodec_alloc_context3(codec);
    av::Ensure(raw, "Failed to allocate decoder context");
    auto ctx = AVCodecContextPtr(raw);

    auto ret = avcodec_parameters_to_context(ctx.get(), st->codecpar);
    av::Assert(ret, "Failed to copy codec parameters to context");

    ret = avcodec_open2(ctx.get(), codec, nullptr);
    av::Assert(ret, "Failed to open decoder");

    if (ctx->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC || ctx->ch_layout.nb_channels == 0) {
        int ch = ctx->ch_layout.nb_channels;
        if (ch == 0) ch = st->codecpar->ch_layout.nb_channels;
        av::Ensure(ch > 0, "No audio channels available in stream");
        av_channel_layout_uninit(&ctx->ch_layout);
        av_channel_layout_default(&ctx->ch_layout, ch);
    }

    return ctx;
}

AVCodecContextPtr OpenEncoder(const TargetFormat &params) {
    auto ectx = AVCodecContextPtr(avcodec_alloc_context3(nullptr));
    av::Ensure(ectx.get(), "Failed to allocate encoder context");
    ectx->codec_type = AVMEDIA_TYPE_AUDIO;
    ectx->codec_id = params.CodecId;
    ectx->sample_rate = params.SampleRate;
    av_channel_layout_default(&ectx->ch_layout, 2);
    ectx->sample_fmt = params.SampleFormat;
    ectx->bit_rate = 0;
    ectx->time_base = AVRational{1, params.SampleRate};
    const auto ret = avcodec_open2(ectx.get(), avcodec_find_encoder(ectx->codec_id), nullptr);
    av::Assert(ret, "Failed to open encoder");
    return ectx;
}

AVStream *OpenOutputStream(const fs::path &path,
                           const AVFormatOutputContextPtr &ofmt,
                           const AVCodecContextPtr &ectx) {
    AVStream *ost = avformat_new_stream(ofmt.get(), nullptr);
    av::Ensure(ost, "Failed to create output stream");
    ost->time_base = ectx->time_base;

    auto ret = avcodec_parameters_from_context(ost->codecpar, ectx.get());
    av::Assert(ret, "Failed to copy codec parameters to output stream");

    if (!(ofmt->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ofmt->pb, PathToUtf8(path).c_str(), AVIO_FLAG_WRITE);
        av::Assert(ret, path, "Failed to open output I/O");
    }

    ret = avformat_write_header(ofmt.get(), nullptr);
    av::Assert(ret, path, "Failed to write output format header");

    return ost;
}

} // namespace Audio::detail
