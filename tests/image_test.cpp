#include "common.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string_view>
#include <vector>

#include "image/detail/chunk.hpp"
#include "image/image.hpp"

using namespace Image;

namespace {

void WriteBytes(const fs::path &path, const std::string_view bytes) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw lib::FileError(path, "Failed to create generated test fixture");
    }
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!out) {
        throw lib::FileError(path, "Failed to write generated test fixture");
    }
}

void EnsureGeneratedPpm(const fs::path &path, const int width, const int height, const int seed) {
    if (std::filesystem::exists(path)) {
        return;
    }

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw lib::FileError(path, "Failed to create generated image fixture");
    }
    out << "P6\n" << width << " " << height << "\n255\n";

    std::vector<char> row(static_cast<size_t>(width) * 3);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const size_t i = static_cast<size_t>(x) * 3;
            row[i] = static_cast<char>((x + seed * 29) & 0xff);
            row[i + 1] = static_cast<char>((y + seed * 47) & 0xff);
            row[i + 2] = static_cast<char>(((x / 2) + (y / 3) + seed * 71) & 0xff);
        }
        out.write(row.data(), static_cast<std::streamsize>(row.size()));
    }
    if (!out) {
        throw lib::FileError(path, "Failed to write generated image fixture");
    }
}

void EnsureGeneratedAfb(const fs::path &path) {
    if (std::filesystem::exists(path)) {
        return;
    }

    WriteBytes(path,
               "AFB_GENERATED_PREFIX_DDS generated placeholder 1_POF0"
               "AFB_GENERATED_MIDDLE_DDS generated placeholder 2_POF0"
               "AFB_GENERATED_SUFFIX");
}

void EnsureImageFixtures() {
    std::filesystem::create_directories(GetInputPath());
    EnsureGeneratedPpm(GetInputPath(L"1.jpg"), 640, 640, 1);
    EnsureGeneratedPpm(GetInputPath(L"2.jpg"), 768, 768, 2);
    EnsureGeneratedPpm(GetInputPath(L"3.jpg"), 896, 896, 3);
    EnsureGeneratedPpm(GetInputPath(L"4.jpg"), 1024, 1024, 4);
    EnsureGeneratedPpm(GetInputPath(L"bg.png"), 1920, 1080, 5);
    EnsureGeneratedAfb(GetInputPath(L"st_dummy.afb"));
}

} // namespace

int main(const int argc, char *argv[]) {
    Setup();
    EnsureImageFixtures();
    Initialize();
    const int ret = Catch::Session().run(argc, argv);
    return ret;
}

TEST_CASE("EnsureValid") {
    REQUIRE_NOTHROW(EnsureValid(GetInputPath(L"1.jpg")));
    REQUIRE_THROWS(EnsureValid(GetInputPath(L"invalid.png")));
    REQUIRE_THROWS(EnsureValid(GetInputPath(L"nonexistent.jpg")));
}

TEST_CASE("ConvertJacket") {
    SECTION("1.jpg") {
        const auto srcPath = GetInputPath(L"1.jpg");
        const auto dstPath = GetOutputPath(L"converted_jacket_1.dds");
        REQUIRE_NOTHROW(ConvertJacket(srcPath, dstPath));
        REQUIRE(std::filesystem::exists(dstPath));
    }

    SECTION("2.jpg") {
        const auto srcPath = GetInputPath(L"2.jpg");
        const auto dstPath = GetOutputPath(L"converted_jacket_2.dds");
        REQUIRE_NOTHROW(ConvertJacket(srcPath, dstPath));
        REQUIRE(std::filesystem::exists(dstPath));
    }

    SECTION("Invalid image") {
        const auto srcPath = GetInputPath(L"invalid.jpg");
        const auto dstPath = GetOutputPath(L"converted_invalid_jacket.dds");
        REQUIRE_THROWS(ConvertJacket(srcPath, dstPath));
        REQUIRE_FALSE(std::filesystem::exists(dstPath));
    }

    SECTION("Produces a DDS file") {
        const auto srcPath = GetInputPath(L"1.jpg");
        const auto dstPath = GetOutputPath(L"jacket_header_check.dds");
        REQUIRE_NOTHROW(ConvertJacket(srcPath, dstPath));

        auto read_all = [](const fs::path &p) {
            std::ifstream in(p, std::ios::binary);
            REQUIRE(in);
            return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), {});
        };
        const auto bytes = read_all(dstPath);
        REQUIRE(bytes.size() > 4);
        REQUIRE(bytes[0] == 'D');
        REQUIRE(bytes[1] == 'D');
        REQUIRE(bytes[2] == 'S');
        REQUIRE(bytes[3] == ' ');
    }
}

TEST_CASE("ConvertStage") {
    const auto stSrcPath = GetInputPath(L"st_dummy.afb");
    SECTION("All") {
        const auto bgSrcPath = GetInputPath(L"bg.png");
        const std::array fxSrcPaths = {GetInputPath(L"1.jpg"), GetInputPath(L"2.jpg"), GetInputPath(L"3.jpg"),
                                       GetInputPath(L"4.jpg")};
        const auto stDstPath = GetOutputPath(L"converted_stage.afb");

        REQUIRE_NOTHROW(ConvertStage(bgSrcPath, stSrcPath, stDstPath, fxSrcPaths));
        REQUIRE(std::filesystem::exists(stDstPath));
    }

    SECTION("Partial") {
        const auto bgSrcPath = GetInputPath(L"bg.png");
        const std::array<std::filesystem::path, 4> fxSrcPaths = {
            GetInputPath(L"1.jpg"), {}, GetInputPath(L"3.jpg"), {}};
        const auto stDstPath = GetOutputPath(L"converted_stage_with_missing_effects.afb");

        REQUIRE_NOTHROW(ConvertStage(bgSrcPath, stSrcPath, stDstPath, fxSrcPaths));

        REQUIRE(std::filesystem::exists(stDstPath));
    }
}

TEST_CASE("ReplaceChunks") {
    const std::vector<uint8_t> data = {'a', 'a', '0', '1', '2', 'b', 'b', '3', '4', '5', 'c', 'c'};
    const std::vector<uint8_t> replacement = {'X', 'X'};
    const auto dstPath = GetOutputPath(L"replace_chunks_streamed.bin");

    REQUIRE_NOTHROW(Image::detail::ReplaceChunks(
        data, dstPath, {{2, 5}, {7, 10}},
        {std::span<const uint8_t>(replacement), std::optional<std::span<const uint8_t>>{}}));

    std::ifstream in(dstPath, std::ios::binary);
    REQUIRE(in);
    const std::vector<uint8_t> bytes(std::istreambuf_iterator<char>(in), {});
    REQUIRE(bytes == std::vector<uint8_t>{'a', 'a', 'X', 'X', 'b', 'b', '3', '4', '5', 'c', 'c'});
}

TEST_CASE("ExtractDdsFromAfb") {
    const auto srcPath = GetInputPath(L"st_dummy.afb");
    const auto dstFolder = GetOutputPath(L"extracted_dds");

    std::filesystem::create_directories(dstFolder);

    REQUIRE_NOTHROW(ExtractDds(srcPath, dstFolder));

    bool foundDdsFile = false;
    for (const auto &entry : std::filesystem::directory_iterator(dstFolder)) {
        if (entry.path().extension() == ".dds") {
            foundDdsFile = true;
            break;
        }
    }

    REQUIRE(foundDdsFile);
}

TEST_CASE("Image performance benchmarks", "[.][!benchmark][image]") {
    const auto jacketSrcPath = GetInputPath(L"1.jpg");
    const auto bgSrcPath = GetInputPath(L"bg.png");
    const auto stSrcPath = GetInputPath(L"st_dummy.afb");
    const std::array fxSrcPaths = {GetInputPath(L"1.jpg"), GetInputPath(L"2.jpg"), GetInputPath(L"3.jpg"),
                                   GetInputPath(L"4.jpg")};

    BENCHMARK("ConvertJacket") {
        const auto dstPath = GetOutputPath(L"benchmark_jacket.dds");
        ConvertJacket(jacketSrcPath, dstPath);
        return std::filesystem::file_size(dstPath);
    };

    BENCHMARK("ConvertStage") {
        const auto dstPath = GetOutputPath(L"benchmark_stage.afb");
        ConvertStage(bgSrcPath, stSrcPath, dstPath, fxSrcPaths);
        return std::filesystem::file_size(dstPath);
    };

    BENCHMARK("ExtractDds") {
        const auto dstFolder = GetOutputPath(L"benchmark_extract_dds");
        std::filesystem::create_directories(dstFolder);
        ExtractDds(stSrcPath, dstFolder);
        return std::distance(std::filesystem::directory_iterator(dstFolder), std::filesystem::directory_iterator());
    };
}
