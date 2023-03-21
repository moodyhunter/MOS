# SPDX-License-Identifier: GPL-3.0-or-later

target_sources(mos_kernel PRIVATE ${CMAKE_CURRENT_LIST_DIR}/startup.c)
set(MOS_BOOTABLE_TARGETS_FILE_multiboot ${CMAKE_BINARY_DIR}/mos_multiboot.bin)
create_bootable_kernel_binary(
    multiboot
    ${CMAKE_CURRENT_LIST_DIR}/multiboot.ld
    ${CMAKE_BINARY_DIR}/mos_multiboot.bin
    ${CMAKE_CURRENT_LIST_DIR}/multiboot.asm
)
