# SPDX-License-Identifier: GPL-3.0-or-later

add_kernel_source(
    riscv64_platform.c
    riscv64_platform_api.c
    sbi/sbi-call.c
    mm/mm.c
    cpu/interrupt.S
    devices/sbi_console.c
)
