# SPDX-License-Identifier: GPL-3.0-or-later

create_bootable_kernel_binary(
    TARGET
        mos_limine
    LINKER_SCRIPT
        limine_entry.ld
    FILENAME
        limine_entry.elf
    SOURCES
        limine_entry.c
)

# create a limine bootable ISO

if (NOT LIMINE_ROOT)
    set(LIMINE_ROOT "/usr/share/limine")
endif()

list(APPEND CMAKE_PROGRAM_PATH ${LIMINE_ROOT}/bin ${LIMINE_ROOT})

find_program(LIMINE_BIN limine NO_CACHE)
find_program(XORRISO xorriso NO_CACHE)

if (NOT LIMINE_BIN OR NOT XORRISO)
    message(WARNING "limine and xorriso are required to build the limine ISO")
    return()
endif()

set(BOOT_DIR ${CMAKE_BINARY_DIR}/boot.dir)
set(LIMINE_BOOT_DIR ${CMAKE_BINARY_DIR}/boot.dir/iso_root/limine)

if (NOT DEFINED LIMINE_CFG)
    set(LIMINE_CFG ${CMAKE_CURRENT_LIST_DIR}/limine.cfg)
endif()

add_custom_target(mos_limine_iso
    COMMAND ${CMAKE_COMMAND} -E make_directory ${LIMINE_BOOT_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy ${LIMINE_ROOT}/limine-bios.sys ${LIMINE_ROOT}/limine-bios-cd.bin ${LIMINE_BOOT_DIR}/
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:mos_limine> ${CMAKE_BINARY_DIR}/initrd.cpio ${LIMINE_BOOT_DIR}/
    COMMAND ${CMAKE_COMMAND} -E copy ${LIMINE_CFG} ${LIMINE_BOOT_DIR}/limine.cfg
    COMMAND ${XORRISO} -as mkisofs -b limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table --protective-msdos-label ${LIMINE_BOOT_DIR} -o ${BOOT_DIR}/mos_limine.iso > xorriso.log 2>&1
    COMMAND ${LIMINE_BIN} bios-install ${BOOT_DIR}/mos_limine.iso > limine.log 2>&1
    BYPRODUCTS
        ${BOOT_DIR}/mos_limine.iso

    DEPENDS
        mos_limine
        mos_initrd
        limine.cfg
        ${LIMINE_ROOT}/limine-bios.sys
        ${LIMINE_ROOT}/limine-bios-cd.bin
)

add_summary_item(BOOTABLE mos_limine_iso "${BOOT_DIR}/mos_limine.iso" "ISO with limine bootloader")
