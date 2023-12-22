# SPDX-License-Identifier: GPL-3.0-or-later

add_mos_library(NAME nanopb
    SOURCES
        nanopb/pb_common.c
        nanopb/pb_common.h
        nanopb/pb_decode.c
        nanopb/pb_decode.h
        nanopb/pb_encode.c
    PUBLIC_INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/nanopb
    DEFINES
        "-DPB_SYSTEM_HEADER=\"nanopb_mos.h\""
)
