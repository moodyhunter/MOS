# SPDX-License-Identifier: GPL-3.0-or-later

include(${CMAKE_CURRENT_LIST_DIR}/boot/uefi/bootable.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/boot/limine/bootable.cmake)

add_kernel_source(
    INCLUDE_DIRECTORIES
    ${CMAKE_CURRENT_LIST_DIR}/include

    SOURCES
        x86_64_platform.c
        x86_64_platform_api.c
)
