#include "audio/audio.hpp"
#include "audio/convert.hpp"
#include "image/image.hpp"
#include "lib.hpp"

#include <CLI/CLI.hpp>
#include <array>
#include <filesystem>
#include <iostream>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#define RET_OK 0
#define RET_ERROR 1
#define RET_NOOP 2

struct {
    fs::path src, dst;
    double offset = 0.0;
} audio_normalize_opts;

struct {
    fs::path src;
} audio_ensure_valid_opts, image_ensure_valid_opts;

struct {
    fs::path src, dst;
} convert_jacket_opts, extract_dds_opts;

struct {
    fs::path bg, stsrc, stdst;
    std::array<fs::path, 4> fx{};
} convert_stage_opts;

template <typename T>
int core(int argc, T **argv) {
    spdlog::set_default_logger(spdlog::stderr_color_mt("Manipulate"));

    CLI::App app;
    int ret = RET_OK;

    std::string log_level = "info";
    app.add_option("--loglevel", log_level, "(trace, debug, info, warn, error, critical, off)")
        ->default_val("info");

    const auto subcmd_audio_normalize = app.add_subcommand("audio_normalize", "Audio::Normalize")->fallthrough();
    subcmd_audio_normalize->add_option("-s,--src", audio_normalize_opts.src)->required();
    subcmd_audio_normalize->add_option("-d,--dst", audio_normalize_opts.dst)->required();
    subcmd_audio_normalize->add_option("-o,--offset", audio_normalize_opts.offset, "offset (s)");

    const auto subcmd_audio_ensure_valid = app.add_subcommand("audio_check", "Audio::EnsureValid")->fallthrough();
    subcmd_audio_ensure_valid->add_option("-s,--src", audio_ensure_valid_opts.src)->required();

    const auto subcmd_image_ensure_valid = app.add_subcommand("image_check", "Image::EnsureValid")->fallthrough();
    subcmd_image_ensure_valid->add_option("-s,--src", image_ensure_valid_opts.src)->required();

    const auto subcmd_convert_jacket = app.add_subcommand("convert_jacket", "Image::ConvertJacket")->fallthrough();
    subcmd_convert_jacket->add_option("-s,--src", convert_jacket_opts.src)->required();
    subcmd_convert_jacket->add_option("-d,--dst", convert_jacket_opts.dst)->required();

    const auto subcmd_convert_stage = app.add_subcommand("convert_stage", "Image::ConvertStage")->fallthrough();
    subcmd_convert_stage->add_option("-b,--bg", convert_stage_opts.bg)->required();
    subcmd_convert_stage->add_option("-s,--stsrc", convert_stage_opts.stsrc)->required();
    subcmd_convert_stage->add_option("-d,--stdst", convert_stage_opts.stdst)->required();
    subcmd_convert_stage->add_option("-1,--fx1", convert_stage_opts.fx[0]);
    subcmd_convert_stage->add_option("-2,--fx2", convert_stage_opts.fx[1]);
    subcmd_convert_stage->add_option("-3,--fx3", convert_stage_opts.fx[2]);
    subcmd_convert_stage->add_option("-4,--fx4", convert_stage_opts.fx[3]);

    const auto subcmd_extract_dds = app.add_subcommand("extract_dds", "Image::ExtractDds")->fallthrough();
    subcmd_extract_dds->add_option("-s,--src", extract_dds_opts.src)->required();
    subcmd_extract_dds->add_option("-d,--dst", extract_dds_opts.dst)->required();

    try {
        app.require_subcommand(1);
        app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
        std::cerr << app.help() << std::endl;
        return app.exit(e);
    }

    spdlog::level::level_enum lvl;
    if (log_level == "trace")    lvl = spdlog::level::trace;
    else if (log_level == "debug")   lvl = spdlog::level::debug;
    else if (log_level == "info")    lvl = spdlog::level::info;
    else if (log_level == "warn")    lvl = spdlog::level::warn;
    else if (log_level == "error")   lvl = spdlog::level::err;
    else if (log_level == "critical")lvl = spdlog::level::critical;
    else if (log_level == "off")     lvl = spdlog::level::off;
    else {
        spdlog::warn("Unknown log level '{}', defaulting to 'info'.", log_level);
        lvl = spdlog::level::info;
    }
    spdlog::set_level(lvl);

    try {
        if (subcmd_audio_normalize->parsed()) {
            Audio::Initialize();
            ret = Audio::Normalize(audio_normalize_opts.src, audio_normalize_opts.dst, audio_normalize_opts.offset) ? RET_OK : RET_NOOP;
        } else if (subcmd_audio_ensure_valid->parsed()) {
            Audio::Initialize();
            Audio::EnsureValid(audio_ensure_valid_opts.src);
        } else if (subcmd_image_ensure_valid->parsed()) {
            Image::Initialize();
            Image::EnsureValid(image_ensure_valid_opts.src);
        } else if (subcmd_convert_jacket->parsed()) {
            Image::Initialize();
            Image::ConvertJacket(convert_jacket_opts.src, convert_jacket_opts.dst);
        } else if (subcmd_convert_stage->parsed()) {
            Image::Initialize();
            Image::ConvertStage(convert_stage_opts.bg, convert_stage_opts.stsrc, convert_stage_opts.stdst, convert_stage_opts.fx);
        } else if (subcmd_extract_dds->parsed()) {
            Image::ExtractDds(extract_dds_opts.src, extract_dds_opts.dst);
        } else {
            throw std::runtime_error("No subcommand specified.");
        }
    } catch (const std::exception &e) {
        spdlog::error(e.what());
        ret = RET_ERROR;
    } catch (...) {
        spdlog::error("Unknown error occurred.");
        ret = RET_ERROR;
    }
    return ret;
}

#ifdef _WIN32
int wmain(const int argc, wchar_t **argv) {
    return core(argc, argv);
}
#else
int main(const int argc, char **argv) {
    return core(argc, argv);
}
#endif