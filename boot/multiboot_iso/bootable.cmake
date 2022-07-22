# SPDX-License-Identifier: GPL-3.0-or-later

set(MOS_ISODIR ${CMAKE_BINARY_DIR}/.isodir)

find_program(GRUB_MKRESCUE grub-mkrescue NO_CACHE)

if (GRUB_MKRESCUE)
    add_custom_target(multiboot_iso
        DEPENDS $<TARGET_FILE:multiboot>
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${MOS_ISODIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${MOS_ISODIR}/boot/grub
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_LIST_DIR}/grub.cfg ${MOS_ISODIR}/boot/grub
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:multiboot> ${MOS_ISODIR}/boot/mos-kernel
        COMMAND ${GRUB_MKRESCUE} -o ${CMAKE_BINARY_DIR}/mos_multiboot_img.iso ${MOS_ISODIR}
    )
    set(MOS_BOOTABLE_TARGETS_FILE_multiboot_iso ${CMAKE_BINARY_DIR}/mos_multiboot_img.iso)
else()
    message(WARNING "grub-mkrescue not found, you cannot create multiboot_iso")
    add_custom_target(multiboot_iso
        COMMAND ${CMAKE_COMMAND} -E echo ""
        COMMAND ${CMAKE_COMMAND} -E echo "============== WARNING WARNING WARNING WARNING =============="
        COMMAND ${CMAKE_COMMAND} -E echo " grub-mkrescue NOT FOUND, not creating mos_multiboot_img.iso "
        COMMAND ${CMAKE_COMMAND} -E echo "============================================================="
        COMMAND ${CMAKE_COMMAND} -E echo ""
    )
    set(MOS_BOOTABLE_TARGETS_FILE_multiboot_iso "grub-mkrescue not found")
endif()

add_dependencies(multiboot_iso multiboot)
