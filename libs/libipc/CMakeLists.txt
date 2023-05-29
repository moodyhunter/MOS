# SPDX-License-Identifier: GPL-3.0-or-later

add_mos_library(
    NAME libipc
    PUBLIC_INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_LIST_DIR}/include
    SOURCES
        libipc.c
)
