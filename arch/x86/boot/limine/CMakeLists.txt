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

