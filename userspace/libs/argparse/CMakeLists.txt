# SPDX-License-Identifier: GPL-3.0-or-later

add_mos_library(
    NAME argparse
    SOURCES
        libargparse.c
    PUBLIC_INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_LIST_DIR}/include
    USERSPACE_ONLY
)

add_to_initrd(TARGET argparse_hosted /lib/)
