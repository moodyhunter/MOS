# SPDX-License-Identifier: GPL-3.0-or-later

add_nasm_binary(mos_multiboot_header SOURCE ${CMAKE_CURRENT_LIST_DIR}/multiboot.asm ELF_OBJECT)
add_kernel_binary(
    LOADER_ASSEMBLY_TARGET mos_multiboot_header::object
    LINKER_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/multiboot.ld)
