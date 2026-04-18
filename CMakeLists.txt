cmake_minimum_required(VERSION 3.28)
project(muautils LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 23)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

add_library(mua_common INTERFACE)
target_link_libraries(mua_common INTERFACE spdlog::spdlog fmt::fmt)
target_include_directories(mua_common INTERFACE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

find_package(FFMPEG REQUIRED)
add_library(mua_audio STATIC
        src/audio/audio.cpp
        src/audio/detail/format.cpp
        src/audio/detail/filter.cpp
        src/audio/detail/pipeline.cpp
        src/audio/detail/analyze.cpp)
target_link_libraries(mua_audio PUBLIC mua_common)

target_include_directories(mua_audio PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_directories(mua_audio PRIVATE ${FFMPEG_LIBRARY_DIRS})
target_link_libraries(mua_audio PRIVATE ${FFMPEG_LIBRARIES})

find_package(directxtex CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(VIPS_CPP REQUIRED IMPORTED_TARGET vips-cpp)
add_library(mua_image STATIC
        src/image/detail/chunk.cpp
        src/image/convert.cpp
        src/image/image.cpp)
target_link_libraries(mua_image PUBLIC mua_common)
target_link_libraries(mua_image PRIVATE Microsoft::DirectXTex PkgConfig::VIPS_CPP)

add_executable(mua src/main.cpp src/cli/cli.cpp)
find_package(CLI11 CONFIG REQUIRED)
target_link_libraries(mua PRIVATE mua_audio mua_image CLI11::CLI11)

set(TEST_ASSET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests/assets")
set(PRIVATE_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/private/include")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/tests/asset.h.in ${PRIVATE_INCLUDE_DIR}/tests/asset.h @ONLY)

add_executable(test_audio tests/audio_test.cpp)
add_executable(test_image tests/image_test.cpp)
find_package(Catch2 CONFIG REQUIRED)
target_link_libraries(test_audio PRIVATE mua_audio Catch2::Catch2 Catch2::Catch2WithMain)
target_link_libraries(test_image PRIVATE mua_image Catch2::Catch2 Catch2::Catch2WithMain)
target_include_directories(test_audio PRIVATE ${PRIVATE_INCLUDE_DIR})
target_include_directories(test_image PRIVATE ${PRIVATE_INCLUDE_DIR})

set(_mua_msvc_targets mua mua_audio mua_image test_audio test_image)
if (MSVC)
    set_target_properties(${_mua_msvc_targets} PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_compile_options(${_mua_msvc_targets} PRIVATE
            "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>"
            "$<$<COMPILE_LANG_AND_ID:C,MSVC>:/utf-8>")
endif ()
unset(_mua_msvc_targets)

install(TARGETS mua
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(TARGETS mua_common
        EXPORT muautilsTargets
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS mua_audio mua_image
        EXPORT muautilsTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        OBJECTS DESTINATION ${CMAKE_INSTALL_LIBDIR}/objects
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT muautilsTargets
        FILE muautilsTargets.cmake
        NAMESPACE muautils::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/muautils)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING
        PATTERN "*.hpp"
        PATTERN "detail" EXCLUDE)

configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/muautilsConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/muautilsConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/muautils)

write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/muautilsConfigVersion.cmake"
        VERSION 1.0.0
        COMPATIBILITY SameMajorVersion)

install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/muautilsConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/muautilsConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/muautils)
