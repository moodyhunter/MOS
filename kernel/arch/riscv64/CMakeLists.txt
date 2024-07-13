# SPDX-License-Identifier: GPL-3.0-or-later

add_kernel_source(
    riscv64_platform.c
    riscv64_platform_api.c
    sbi/sbi-call.c
    mm/mm.c
    cpu/trap.c
    cpu/plic.c
    cpu/interrupt.S
    cpu/context_switch.S
    devices/sbi_console.c
    devices/uart_driver.c
)
