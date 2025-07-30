#include "audio/audio.hpp"
#include "image/image.hpp"
#include "lib.hpp"

#include <CLI/CLI.hpp>
#include <array>
#include <filesystem>
#include <iostream>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#define RET_OK 0
#define RET_ERROR 1
#define RET_NOOP 2

struct {
    fs::path src, dst;
    double offset = 0.0;
} an;

struct {
    fs::path src;
} ai, ii;

struct {
    fs::path src, dst;
} cj, ed;

struct {
    fs::path bg, stsrc, stdst;
    std::array<fs::path, 4> fx{};
} cs;

template <typename T>
int core(int argc, T **argv) {
    spdlog::set_default_logger(spdlog::stderr_color_mt("Manipulate"));

    CLI::App app;
    int ret = RET_OK;

    std::string log_level = "info";
    app.add_option("--loglevel", log_level, "(trace, debug, info, warn, error, critical, off)")
        ->default_val("info");

    const auto scAn = app.add_subcommand("an", "Audio::Normalize")->fallthrough();
    scAn->add_option("-s,--src", an.src)->required();
    scAn->add_option("-d,--dst", an.dst)->required();
    scAn->add_option("-o,--offset", an.offset, "offset (s)");

    const auto scAi = app.add_subcommand("ai", "Audio::EnsureValid")->fallthrough();
    scAi->add_option("-s,--src", ai.src)->required();

    const auto scIi = app.add_subcommand("ii", "Image::EnsureValid")->fallthrough();
    scIi->add_option("-s,--src", ii.src)->required();

    const auto scCj = app.add_subcommand("cj", "Image::ConvertJacket")->fallthrough();
    scCj->add_option("-s,--src", cj.src)->required();
    scCj->add_option("-d,--dst", cj.dst)->required();

    const auto scCs = app.add_subcommand("cs", "Image::ConvertStage")->fallthrough();
    scCs->add_option("-b,--bg", cs.bg)->required();
    scCs->add_option("-s,--stsrc", cs.stsrc)->required();
    scCs->add_option("-d,--stdst", cs.stdst)->required();
    scCs->add_option("-1,--fx1", cs.fx[0]);
    scCs->add_option("-2,--fx2", cs.fx[1]);
    scCs->add_option("-3,--fx3", cs.fx[2]);
    scCs->add_option("-4,--fx4", cs.fx[3]);

    const auto scEd = app.add_subcommand("ed", "Image::ExtractDds")->fallthrough();
    scEd->add_option("-s,--src", ed.src)->required();
    scEd->add_option("-d,--dst", ed.dst)->required();

    try {
        app.require_subcommand(1);
        app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
        std::cerr << app.help() << std::endl;
        return app.exit(e);
    }
    spdlog::level::level_enum lvl = spdlog::level::info;
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
        if (scAn->parsed()) {
            Audio::Initialize();
            ret = Audio::Normalize(an.src, an.dst, an.offset) ? RET_OK : RET_NOOP;
        } else if (scAi->parsed()) {
            Audio::Initialize();
            Audio::EnsureValid(ai.src);
        } else if (scIi->parsed()) {
            Image::Initialize();
            Image::EnsureValid(ii.src);
        } else if (scCj->parsed()) {
            Image::Initialize();
            Image::ConvertJacket(cj.src, cj.dst);
        } else if (scCs->parsed()) {
            Image::Initialize();
            Image::ConvertStage(cs.bg, cs.stsrc, cs.stdst, cs.fx);
        } else if (scEd->parsed()) {
            Image::ExtractDds(ed.src, ed.dst);
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