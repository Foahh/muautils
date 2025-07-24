#include "common.hpp"

#include "audio/audio.hpp"

#include <iostream>

using namespace Audio;

int main(const int argc, char *argv[]) {
    Setup();
    Initialize();
    const int ret = Catch::Session().run(argc, argv);
    return ret;
}

TEST_CASE("EnsureValid") {
    SECTION("Valid audio file") { REQUIRE_NOTHROW(EnsureValid(GetInputPath(L"1.mp3"))); }
    SECTION("Invalid audio file") { REQUIRE_THROWS(EnsureValid(GetInputPath(L"a"))); }
}

void PrintMeta(const AudioStreamMeta &meta) {
    std::cout << "Audio Stream Metadata:" << std::endl;
    std::cout << "StreamIndex: " << meta.StreamIndex << std::endl;
    std::cout << "MediaType:   " << meta.MediaType << std::endl;
    std::cout << "CodecId:     " << meta.CodecId << std::endl;
    std::cout << "SampleFormat:" << meta.SampleFormat << std::endl;
    std::cout << "SampleRate:  " << meta.SampleRate << std::endl;
    std::cout << "Channels:    " << meta.Channels << std::endl;
    std::cout << "Loudness:    " << meta.Loudness << std::endl;
    std::cout << "TruePeak:    " << meta.TruePeak << std::endl;
}

TEST_CASE("Analyze") {
    const auto meta = Analyze(GetInputPath(L"1.mp3"));
    PrintMeta(meta);
}

TEST_CASE("Normalize") {
    const auto srcPath = GetInputPath(L"1.mp3");
    const auto dstPath = GetOutputPath(L"test1_normalized.wav");
    const auto tmpPath = GetOutputPath(L"test1_impossible.wav");

    SECTION("Normalize to FMT_PCM_S16LE_8LU") {
        bool ret = Normalize(srcPath, dstPath, 0.0);
        REQUIRE(ret);

        REQUIRE(fs::exists(dstPath));
        REQUIRE(fs::file_size(dstPath) > 0);

        const auto meta = Analyze(dstPath);
        REQUIRE(meta.SampleRate == FMT_PCM_S16LE_8LU.SampleRate);
        REQUIRE(meta.Channels == FMT_PCM_S16LE_8LU.Channels);
        REQUIRE(meta.SampleFormat == FMT_PCM_S16LE_8LU.SampleFormat);

        ret = Normalize(dstPath, tmpPath, 0.0);
        REQUIRE_FALSE(ret);
        REQUIRE(!fs::exists(tmpPath));
    }
}

TEST_CASE("Normalize HCA") {
    const auto srcPath = GetInputPath(L"1.mp3");
    const auto tmpPath = GetOutputPath(L"1_hca_normalized.wav");
    const auto dstPath = GetOutputPath(L"1_hca_normalized.hca");
    REQUIRE_NOTHROW(NormalizeHca(srcPath, tmpPath, 0.0, dstPath, 32931609366120192UL));
}
