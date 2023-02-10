# SPDX-License-Identifier: GPL-3.0-or-later

target_sources(mos_kernel PRIVATE ${CMAKE_CURRENT_LIST_DIR}/startup.c)

prepare_bootable_kernel_binary(multiboot
    LOADER_ASM ${CMAKE_CURRENT_LIST_DIR}/multiboot.asm
    LINKER_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/multiboot.ld)

add_custom_command(TARGET multiboot
    POST_BUILD
    COMMAND
        ${CMAKE_COMMAND} -E copy $<TARGET_FILE:multiboot> ${CMAKE_BINARY_DIR}/mos_multiboot.bin
    BYPRODUCTS ${CMAKE_BINARY_DIR}/mos_multiboot.bin
)

set(MOS_BOOTABLE_TARGETS_FILE_multiboot ${CMAKE_BINARY_DIR}/mos_multiboot.bin)
