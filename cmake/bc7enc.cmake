set(_bc7enc_root "${CMAKE_SOURCE_DIR}/third_party/bc7enc_rdo")

if (NOT EXISTS "${_bc7enc_root}/rdo_bc_encoder.cpp")
    message(FATAL_ERROR
        "bc7enc_rdo submodule is missing at ${_bc7enc_root}. "
        "Run: git submodule update --init --recursive")
endif ()

add_library(bc7enc OBJECT
        "${_bc7enc_root}/bc7enc.cpp"
        "${_bc7enc_root}/bc7decomp.cpp"
        "${_bc7enc_root}/bc7decomp_ref.cpp"
        "${_bc7enc_root}/rgbcx.cpp"
        "${_bc7enc_root}/ert.cpp"
        "${_bc7enc_root}/utils.cpp"
        "${_bc7enc_root}/lodepng.cpp"
        "${_bc7enc_root}/rdo_bc_encoder.cpp")

target_include_directories(bc7enc SYSTEM PUBLIC "${_bc7enc_root}")

target_compile_definitions(bc7enc PUBLIC SUPPORT_BC7E=0)

if (MSVC)
    target_compile_options(bc7enc PRIVATE /w /FI"cstdint")
else ()
    # bc7enc_rdo relies on <cstdint> being transitively included; newer
    # libstdc++ (GCC >= 13) no longer does this, so force-include it.
    target_compile_options(bc7enc PRIVATE
            -fno-strict-aliasing
            -w
            -include cstdint)
endif ()

set_target_properties(bc7enc PROPERTIES POSITION_INDEPENDENT_CODE ON)

find_package(OpenMP)
if (OpenMP_CXX_FOUND)
    target_link_libraries(bc7enc PUBLIC OpenMP::OpenMP_CXX)
endif ()

unset(_bc7enc_root)
