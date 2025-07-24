#pragma once

#ifdef WIN32
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#define CATCH_CONFIG_WINDOWS_CRTDBG
#endif

#define CATCH_CONFIG_ENABLE_BENCHMARKING
#define CATCH_CONFIG_MAIN

#include <catch2/catch_all.hpp>
#include <filesystem>
#include <string>

#include "lib.hpp"
#include "tests/asset.h"

inline fs::path GetPath(const std::wstring& filename, const std::wstring& subdir = L"") {
    auto base = fs::path(TEST_ASSET_DIR) / subdir;
    return filename.empty() ? base : base / filename;
}

inline fs::path GetInputPath(const std::wstring& filename = L"") {
    return GetPath(filename);
}

inline fs::path GetOutputPath(const std::wstring& filename = L"") {
    return GetPath(filename, L"tmp");
}

inline void Setup()
{
#ifdef WIN32
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    if (const fs::path tmp = GetInputPath(); !std::filesystem::exists(tmp))
    {
        std::filesystem::create_directories(tmp);
    }
}
