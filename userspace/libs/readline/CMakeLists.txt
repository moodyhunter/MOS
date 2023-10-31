# SPDX-License-Identifier: GPL-3.0-or-later

add_mos_library(
    NAME libreadline
    SOURCES
        libreadline.c
    PUBLIC_INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_LIST_DIR}/include
    USERSPACE_ONLY
    HOSTED_ONLY
)

add_to_initrd(TARGET libreadline_hosted /lib/)
