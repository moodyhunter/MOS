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

list(APPEND CMAKE_PROGRAM_PATH ${LIMINE_ROOT}/bin)

find_program(LIMINE_DEPLOY limine-deploy REQUIRED)
find_program(XORRISO xorriso REQUIRED)

set(BOOT_DIR ${CMAKE_BINARY_DIR}/boot.dir)
set(LIMINE_BOOT_DIR ${CMAKE_BINARY_DIR}/boot.dir/iso_root/limine)

add_custom_command(
    OUTPUT ${BOOT_DIR}/mos_limine.iso
    COMMAND mkdir -p ${LIMINE_BOOT_DIR}
    COMMAND cp ${LIMINE_ROOT}/limine.sys ${LIMINE_ROOT}/limine-cd.bin ${LIMINE_BOOT_DIR}/
    COMMAND cp $<TARGET_FILE:mos_limine> ${CMAKE_BINARY_DIR}/initrd.cpio ${CMAKE_CURRENT_LIST_DIR}/limine.cfg ${LIMINE_BOOT_DIR}/
    COMMAND ${XORRISO} -as mkisofs -b limine-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table --protective-msdos-label ${LIMINE_BOOT_DIR} -o ${BOOT_DIR}/mos_limine.iso
    COMMAND ${LIMINE_DEPLOY} ${BOOT_DIR}/mos_limine.iso
    DEPENDS
        mos_limine
        mos_initrd
        limine.cfg
    VERBATIM
    BYPRODUCTS
        ${LIMINE_BOOT_DIR}/limine.sys
        ${LIMINE_BOOT_DIR}/limine-cd.bin
        ${LIMINE_BOOT_DIR}/limine_entry.elf
        ${LIMINE_BOOT_DIR}/initrd.cpio
        ${LIMINE_BOOT_DIR}/limine.cfg
)

add_custom_target(mos_limine_iso DEPENDS ${BOOT_DIR}/mos_limine.iso)
