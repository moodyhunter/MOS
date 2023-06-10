# SPDX-License-Identifier: GPL-3.0-or-later

create_bootable_kernel_binary(
    mos_uefi_entry
    ${CMAKE_CURRENT_LIST_DIR}/uefi_entry.ld
    mos_uefi.elf
    ${CMAKE_CURRENT_LIST_DIR}/uefi_entry.c
)
