# SPDX-License-Identifier: GPL-3.0-or-later

create_bootable_kernel_binary(
    TARGET
        mos_multiboot
    LINKER_SCRIPT
        multiboot.ld
    FILENAME
        multiboot.bin
    SOURCES
        multiboot_entry.c
        multiboot.asm
        startup.c
)

find_program(GRUB_MKRESCUE grub-mkrescue NO_CACHE)

if(GRUB_MKRESCUE)
    set(MOS_ISODIR ${CMAKE_BINARY_DIR}/.multiboot_isodir)
    message(STATUS "Found grub-mkrescue at: ${GRUB_MKRESCUE}")
    add_custom_target(mos_multiboot_iso
        BYPRODUCTS ${CMAKE_BINARY_DIR}/mos_multiboot_img.iso
        DEPENDS $<TARGET_FILE:mos_multiboot> mos_initrd
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${MOS_ISODIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${MOS_ISODIR}/boot/grub
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_LIST_DIR}/grub.cfg ${MOS_ISODIR}/boot/grub
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:mos_multiboot> ${MOS_ISODIR}/boot/mos-kernel
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/initrd.cpio ${MOS_ISODIR}/boot/initrd.cpio
        COMMAND ${GRUB_MKRESCUE} -o ${CMAKE_BINARY_DIR}/mos_multiboot_img.iso ${MOS_ISODIR}
    )
    add_summary_item(BOOTABLE mos_multiboot_iso "" "ISO image for multiboot v1 kernel")
else()
    message(WARNING "grub-mkrescue is not found, you cannot create multiboot_iso")
    add_custom_target(multiboot_iso
        COMMAND ${CMAKE_COMMAND} -E echo ""
        COMMAND ${CMAKE_COMMAND} -E echo "============================================================="
        COMMAND ${CMAKE_COMMAND} -E echo " grub-mkrescue NOT FOUND, not creating mos_multiboot_img.iso "
        COMMAND ${CMAKE_COMMAND} -E echo "============================================================="
        COMMAND ${CMAKE_COMMAND} -E echo ""
    )
endif()

add_dependencies(mos_multiboot_iso mos_multiboot)
