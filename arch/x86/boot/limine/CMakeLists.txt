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

find_program(LIMINE_DEPLOY limine-deploy REQUIRED)
find_program(XORRISO xorriso REQUIRED)

add_custom_command(
    OUTPUT
        ${CMAKE_BINARY_DIR}/boot.dir/mos_limine.iso
    COMMAND
        MOS_BUILD_DIR=${CMAKE_BINARY_DIR} MOS_SRC_DIR=${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_LIST_DIR}/create_iso.sh
    DEPENDS
        mos_limine
        mos_initrd
        limine.cfg
    VERBATIM
)

add_custom_target(
    mos_limine_iso
    DEPENDS
        ${CMAKE_BINARY_DIR}/boot.dir/mos_limine.iso
)
