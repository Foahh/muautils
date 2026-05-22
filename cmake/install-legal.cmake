# Install LICENSE, NOTICE, SOURCE-OFFER, and legal/licenses/ with the package.

set(_mua_legal_src "${CMAKE_CURRENT_SOURCE_DIR}/legal")
set(_mua_legal_dest "${CMAKE_INSTALL_DOCDIR}")

if(NOT EXISTS "${_mua_legal_src}/NOTICE")
    message(FATAL_ERROR "legal/NOTICE not found; run scripts/collect-licenses.ps1 if licenses/ is missing.")
endif()

install(
    FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
        "${_mua_legal_src}/NOTICE"
        "${_mua_legal_src}/SOURCE-OFFER.txt"
    DESTINATION "${_mua_legal_dest}"
)

if(EXISTS "${_mua_legal_src}/licenses")
    install(
        DIRECTORY "${_mua_legal_src}/licenses/"
        DESTINATION "${_mua_legal_dest}/licenses"
        FILES_MATCHING
        PATTERN "*"
    )
else()
    message(WARNING "legal/licenses/ not found. Run scripts/collect-licenses.ps1 before install.")
endif()
