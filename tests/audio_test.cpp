#include "common.hpp"

#include "audio/audio.hpp"
#include "audio/detail/analyze.hpp"
#include "audio/detail/target_format.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <vector>

using namespace Audio;
using namespace Audio::detail;

int main(const int argc, char *argv[]) {
    Setup();
    Initialize();
    const int ret = Catch::Session().run(argc, argv);
    return ret;
}

TEST_CASE("EnsureValid") {
    SECTION("Valid audio file") {
        REQUIRE_NOTHROW(EnsureValid(GetInputPath(L"test.mp3")));
    }
    SECTION("Invalid audio file") {
        REQUIRE_THROWS(EnsureValid(GetInputPath(L"a")));
    }
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

void WriteScaledPcm16Wav(const fs::path &srcPath, const fs::path &dstPath, const double gain) {
    std::ifstream in(srcPath, std::ios::binary);
    REQUIRE(in);

    std::vector<uint8_t> bytes(std::istreambuf_iterator<char>(in), {});
    REQUIRE(bytes.size() > 44);
    REQUIRE(bytes[0] == 'R');
    REQUIRE(bytes[1] == 'I');
    REQUIRE(bytes[2] == 'F');
    REQUIRE(bytes[3] == 'F');
    REQUIRE(bytes[8] == 'W');
    REQUIRE(bytes[9] == 'A');
    REQUIRE(bytes[10] == 'V');
    REQUIRE(bytes[11] == 'E');

    const auto readU32 = [&](const size_t pos) {
        return static_cast<uint32_t>(bytes[pos]) | (static_cast<uint32_t>(bytes[pos + 1]) << 8) |
               (static_cast<uint32_t>(bytes[pos + 2]) << 16) | (static_cast<uint32_t>(bytes[pos + 3]) << 24);
    };

    bool foundData = false;
    for (size_t pos = 12; pos + 8 <= bytes.size();) {
        const uint32_t chunkSize = readU32(pos + 4);
        const size_t chunkData = pos + 8;
        REQUIRE(chunkData + chunkSize <= bytes.size());

        if (bytes[pos] == 'd' && bytes[pos + 1] == 'a' && bytes[pos + 2] == 't' && bytes[pos + 3] == 'a') {
            foundData = true;
            for (size_t samplePos = chunkData; samplePos + 1 < chunkData + chunkSize; samplePos += 2) {
                const auto lo = static_cast<uint16_t>(bytes[samplePos]);
                const auto hi = static_cast<uint16_t>(bytes[samplePos + 1]) << 8;
                const auto sample = static_cast<int16_t>(lo | hi);
                const auto scaled = std::clamp(static_cast<int>(std::lround(sample * gain)),
                                               static_cast<int>(std::numeric_limits<int16_t>::min()),
                                               static_cast<int>(std::numeric_limits<int16_t>::max()));
                bytes[samplePos] = static_cast<uint8_t>(scaled & 0xff);
                bytes[samplePos + 1] = static_cast<uint8_t>((scaled >> 8) & 0xff);
            }
            break;
        }

        pos = chunkData + chunkSize + (chunkSize & 1);
    }
    REQUIRE(foundData);

    std::ofstream out(dstPath, std::ios::binary);
    REQUIRE(out);
    out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    REQUIRE(out.good());
}

TEST_CASE("Analyze") {
    const auto meta = Analyze(GetInputPath(L"test.mp3"));
    PrintMeta(meta);
}

TEST_CASE("Normalize") {
    const auto srcPath = GetOutputPath(L"test1_quiet.wav");
    const auto dstPath = GetOutputPath(L"test1_normalized.wav");
    const auto tmpPath = GetOutputPath(L"test1_impossible.wav");

    std::filesystem::remove(dstPath);
    std::filesystem::remove(tmpPath);
    WriteScaledPcm16Wav(GetInputPath(L"test.wav"), srcPath, 0.25);

    SECTION("Normalize to FMT_PCM_S16LE at reference loudness") {
        const auto sourceMeta = Analyze(srcPath);
        REQUIRE(sourceMeta.Loudness < -15.0);

        bool ret = Normalize(srcPath, dstPath, 0.0);
        REQUIRE(ret);

        REQUIRE(fs::exists(dstPath));
        REQUIRE(fs::file_size(dstPath) > 0);

        const auto meta = Analyze(dstPath);
        REQUIRE(meta.SampleRate == Audio::detail::DefaultTarget.SampleRate);
        REQUIRE(meta.Channels == 2);
        REQUIRE(meta.SampleFormat == Audio::detail::DefaultTarget.SampleFormat);
        REQUIRE(meta.Loudness == Catch::Approx(-8.5).margin(0.2));

        ret = Normalize(dstPath, tmpPath, 0.0);
        REQUIRE_FALSE(ret);
        REQUIRE(!fs::exists(tmpPath));
    }
}
