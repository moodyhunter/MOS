# SPDX-License-Identifier: GPL-3.0-or-later

add_mos_library(
    NAME config
    SOURCES
        libconfig.c
    PUBLIC_INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_LIST_DIR}/include
    USERSPACE_ONLY
)

add_to_initrd(TARGET config /lib/)
