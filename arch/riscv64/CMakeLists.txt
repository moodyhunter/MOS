# SPDX-License-Identifier: GPL-3.0-or-later

add_subdirectory(boot/simple)

add_kernel_source(
    INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_LIST_DIR}/include/private
    PUBLIC_INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_LIST_DIR}/include/public
    SOURCES
        riscv64_platform.c
        riscv64_platform_api.c
)
