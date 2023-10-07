// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/mos_global.h>
#include <mos/platform/platform.h>

#define X86_BIOS_MEMREGION_PADDR 0xf0000
#define BIOS_MEMREGION_SIZE      0x10000

#define X86_EBDA_MEMREGION_PADDR 0x80000
#define EBDA_MEMREGION_SIZE      0x20000

#define X86_VIDEO_DEVICE_PADDR 0xb8000

typedef struct _platform_regs
{
    reg_t r15, r14, r13, r12, r11, r10, r9, r8;
    reg_t di, si, bp, dx, cx, bx, ax;
    reg_t interrupt_number, error_code;
    // iret params
    reg_t ip, cs;
    reg_t eflags;
    reg_t sp, ss;
} __packed platform_regs_t;

MOS_STATIC_ASSERT(sizeof(platform_regs_t) == 176, "platform_regs_t has incorrect size");

// defined in the linker script 'multiboot.ld'
extern const char __MOS_KERNEL_CODE_START[], __MOS_KERNEL_CODE_END[];     // Kernel text
extern const char __MOS_KERNEL_RODATA_START[], __MOS_KERNEL_RODATA_END[]; // Kernel rodata
extern const char __MOS_KERNEL_RW_START[], __MOS_KERNEL_RW_END[];         // Kernel read-write data
extern const char __MOS_KERNEL_END[];                                     // Kernel end

extern mos_platform_info_t x86_platform;
