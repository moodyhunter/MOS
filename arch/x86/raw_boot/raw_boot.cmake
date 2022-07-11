# SPDX-License-Identifier: GPL-3.0-or-later

add_nasm_binary(mos_kernel_loader SOURCE ${CMAKE_CURRENT_LIST_DIR}/kernel_entry.asm ELF_OBJECT)
add_kernel_binary(
    LOADER_ASSEMBLY_TARGET mos_kernel_loader::object
    LINKER_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/raw_boot.ld)

add_nasm_binary(mos_bootsector SOURCE ${CMAKE_CURRENT_LIST_DIR}/boot_sector.asm)
add_custom_command(
    COMMENT "Creating MOS raw kernel image."
    VERBATIM
    OUTPUT ${CMAKE_BINARY_DIR}/mos_kernel.img
    DEPENDS mos_bootsector ${CMAKE_BINARY_DIR}/mos_kernel.bin
    COMMAND
        cat $<TARGET_OBJECTS:mos_bootsector::binary> ${CMAKE_BINARY_DIR}/mos_kernel.bin > mos_kernel.img
)

add_custom_target(mos_kernel_image ALL DEPENDS ${CMAKE_BINARY_DIR}/mos_kernel.img)
