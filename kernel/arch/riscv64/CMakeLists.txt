# SPDX-License-Identifier: GPL-3.0-or-later

add_kernel_source(
    riscv64_platform.cpp
    riscv64_platform_api.cpp
    sbi/sbi-call.cpp
    mm/mm.cpp
    cpu/trap.cpp
    cpu/plic.cpp
    cpu/interrupt.S
    cpu/context_switch.S
    devices/sbi_console.cpp
    devices/uart_driver.cpp
)
