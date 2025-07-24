#include "common.hpp"

#include <filesystem>
#include <fstream>

#include "image/image.hpp"

using namespace Image;

int main(const int argc, char *argv[]) {
    Setup();
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
}

TEST_CASE("ConvertStage") {
    const auto nfDstPath = GetOutputPath(L"converted_nf.afb");
    SECTION("All") {
        const auto bgSrcPath = GetInputPath(L"bg.png");
        const std::array fxSrcPaths = {
                GetInputPath(L"1.jpg"), GetInputPath(L"2.jpg"), GetInputPath(L"3.jpg"), GetInputPath(L"4.jpg")};
        const auto stDstPath = GetOutputPath(L"converted_stage.afb");

        REQUIRE_NOTHROW(ConvertStage(bgSrcPath, nfDstPath, stDstPath, fxSrcPaths));
        REQUIRE(std::filesystem::exists(stDstPath));
    }

    SECTION("Partial") {
        const auto bgSrcPath = GetInputPath(L"bg.png");
        const std::array<std::filesystem::path, 4> fxSrcPaths = {GetInputPath(L"1.jpg"), {},
                                                                                GetInputPath(L"3.jpg"), {}};
        const auto stDstPath = GetOutputPath(L"converted_stage_with_missing_effects.afb");

        REQUIRE_NOTHROW(ConvertStage(bgSrcPath, nfDstPath, stDstPath, fxSrcPaths));

        REQUIRE(std::filesystem::exists(stDstPath));
    }
}

TEST_CASE("ExtractDdsFromAfb") {
    const auto srcPath = GetInputPath(L"1.afb");
    const auto dstFolder = GetOutputPath(L"extracted_dds");

    std::filesystem::create_directories(dstFolder);

    REQUIRE_NOTHROW(ExtractDds(srcPath, dstFolder));

    bool foundDdsFile = false;
    for (const auto &entry: std::filesystem::directory_iterator(dstFolder)) {
        if (entry.path().extension() == ".dds") {
            foundDdsFile = true;
            break;
        }
    }

    REQUIRE(foundDdsFile);
}
