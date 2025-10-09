cmake_minimum_required(VERSION 3.28)
project(muautils LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 23)

include_directories("src")

if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
endif ()

# Common Library
add_library(common_lib INTERFACE)
find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
target_link_libraries(common_lib INTERFACE spdlog::spdlog fmt::fmt)

# Audio Library
add_library(audio_lib INTERFACE)
target_sources(audio_lib PUBLIC src/audio/convert.cpp src/audio/audio.cpp)
find_package(FFMPEG REQUIRED)
target_include_directories(audio_lib INTERFACE ${FFMPEG_INCLUDE_DIRS})
target_link_directories(audio_lib INTERFACE ${FFMPEG_LIBRARY_DIRS})
target_link_libraries(audio_lib INTERFACE ${FFMPEG_LIBRARIES} common_lib)

# Image Library
add_library(image_lib INTERFACE)
target_sources(image_lib PUBLIC src/image/convert.cpp src/image/image.cpp)
find_package(directxtex CONFIG REQUIRED)
find_package(freeimage CONFIG REQUIRED)
target_link_libraries(image_lib INTERFACE common_lib Microsoft::DirectXTex freeimage::FreeImage freeimage::FreeImagePlus)

# Main Executables
add_executable(mua src/main.cpp
        src/audio/convert.hpp
)
find_package(CLI11 CONFIG REQUIRED)
target_link_libraries(mua PRIVATE audio_lib image_lib CLI11::CLI11)

# Test Executables
set(TEST_ASSET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests/assets")
set(PRIVATE_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/private/include")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/tests/asset.h.in ${PRIVATE_INCLUDE_DIR}/tests/asset.h @ONLY)
include_directories(${PRIVATE_INCLUDE_DIR})

add_executable(test_audio tests/audio_test.cpp)
add_executable(test_image tests/image_test.cpp)

find_package(Catch2 CONFIG REQUIRED)
target_link_libraries(test_audio PRIVATE audio_lib Catch2::Catch2 Catch2::Catch2WithMain)
target_link_libraries(test_image PRIVATE image_lib Catch2::Catch2 Catch2::Catch2WithMain)