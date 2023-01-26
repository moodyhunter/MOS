# SPDX-License-Identifier: GPL-3.0-or-later

add_kernel_source(
    INCLUDE_DIRECTORIES
    ${CMAKE_CURRENT_LIST_DIR}/include
    RELATIVE_SOURCES
    x86_64_platform.c
)
