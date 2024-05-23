# SPDX-License-Identifier: GPL-3.0-or-later

create_bootable_kernel_binary(
    TARGET
        mos_riscv_simple
    LINKER_SCRIPT
        riscv_simple.ld
    FILENAME
        riscv_simple.elf
    SOURCES
        entry.S entry.cpp fdt_dump.cpp
)
