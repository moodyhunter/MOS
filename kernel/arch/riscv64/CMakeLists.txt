# SPDX-License-Identifier: GPL-3.0-or-later

add_kernel_source(
    riscv64_platform.c
    riscv64_platform_api.c
    sbi/sbi-call.c
    devices/sbi_console.c
)
