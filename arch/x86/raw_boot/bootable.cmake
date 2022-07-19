# SPDX-License-Identifier: GPL-3.0-or-later

set(MOS_KERNEL_IMAGE ${CMAKE_BINARY_DIR}/mos_kernel.img)

prepare_bootable_kernel_binary(mos_raw_boot_kernel_bin
    LOADER_ASM ${CMAKE_CURRENT_LIST_DIR}/kernel_entry.asm
    LINKER_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/raw_boot.ld)

add_nasm_binary(mos_bootsector SOURCE ${CMAKE_CURRENT_LIST_DIR}/boot_sector.asm)
add_custom_command(
    COMMENT "Creating MOS kernel raw disk image."
    VERBATIM
    OUTPUT ${MOS_KERNEL_IMAGE}
    DEPENDS mos_bootsector mos_raw_boot_kernel_bin
    COMMAND
        cat $<TARGET_OBJECTS:mos_bootsector::binary> $<TARGET_FILE:mos_raw_boot_kernel_bin> > ${MOS_KERNEL_IMAGE}
)

add_custom_target(raw_boot DEPENDS ${MOS_KERNEL_IMAGE})
set(MOS_BOOTABLE_TARGETS_FILE_raw_boot ${MOS_KERNEL_IMAGE})
