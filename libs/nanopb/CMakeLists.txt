# SPDX-License-Identifier: GPL-3.0-or-later

add_mos_library(NAME nanopb
    SOURCES
        nanopb/pb_common.c
        nanopb/pb_common.h
        nanopb/pb_decode.c
        nanopb/pb_decode.h
        nanopb/pb_encode.c
    PUBLIC_INCLUDE_DIRECTORIES
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/nanopb>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
    PUBLIC_HEADERS
        nanopb_mos.h
    PUBLIC_HEADER_SET
        nanopb/pb.h
        nanopb/pb_decode.h
        nanopb/pb_encode.h
    PUBLIC_HEADER_SET_BASE
        nanopb
    DEFINES
        "-DPB_SYSTEM_HEADER=\"nanopb_mos.h\""
)
