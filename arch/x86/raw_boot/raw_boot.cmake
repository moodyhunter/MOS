# SPDX-License-Identifier: GPL-3.0-or-later

add_nasm_binary(mos_kernel_loader SOURCE ${CMAKE_CURRENT_LIST_DIR}/kernel_entry.asm ELF_OBJECT)
set(mos_kernel_objects_input "$<TARGET_OBJECTS:mos_kernel_loader::object>;$<TARGET_OBJECTS:mos::kernel_object>")
set(mos_raw_boot_linker_script ${CMAKE_CURRENT_LIST_DIR}/raw_boot.ld)
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/mos_kernel.bin
    COMMENT "Linking the MOS kernel object..."
    DEPENDS mos_kernel_loader mos::kernel_object
    VERBATIM
    COMMAND
        sh -c "export OBJS='${mos_kernel_objects_input}' && ld -melf_i386 -o ${CMAKE_BINARY_DIR}/mos_kernel.bin -T${mos_raw_boot_linker_script} \${OBJS//;/ }"
)

add_nasm_binary(mos_bootsector SOURCE ${CMAKE_CURRENT_LIST_DIR}/boot_sector.asm)
add_custom_command(
    COMMENT "Creating MOS raw kernel image."
    VERBATIM
    OUTPUT ${CMAKE_BINARY_DIR}/mos_kernel.img
    DEPENDS mos_bootsector ${CMAKE_BINARY_DIR}/mos_kernel.bin
    COMMAND
        cat $<TARGET_OBJECTS:mos_bootsector::binary> ${CMAKE_BINARY_DIR}/mos_kernel.bin > mos-kernel.img
)
