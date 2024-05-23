# SPDX-License-Identifier: GPL-3.0-or-later

if (MOS_ARCH STREQUAL "riscv64")
    add_mos_library(
        NAME libfdt
        SOURCES
            libfdt/fdt.c
            libfdt/fdt_addresses.c
            libfdt/fdt_empty_tree.c
            libfdt/fdt_overlay.c
            libfdt/fdt_ro.c
            libfdt/fdt_rw.c
            libfdt/fdt_strerror.c
            libfdt/fdt_sw.c
            libfdt/fdt_wip.c
            libfdt++.cpp
        KERNEL_ONLY
        PUBLIC_INCLUDE_DIRECTORIES
            ${CMAKE_CURRENT_SOURCE_DIR}/libfdt/
            ${CMAKE_CURRENT_SOURCE_DIR}/include/
    )
endif()
