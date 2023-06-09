# SPDX-License-Identifier: GPL-3.0-or-later

create_bootable_kernel_binary(
    mos_limine_entry
    ${CMAKE_CURRENT_LIST_DIR}/limine_entry.ld
    mos_limine_entry.elf
    ${CMAKE_CURRENT_LIST_DIR}/limine_entry.c
)
