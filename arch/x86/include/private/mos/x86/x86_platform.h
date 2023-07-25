// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/x86/devices/serial_console.h"

#include <mos/mos_global.h>
#include <mos/platform/platform.h>

#define X86_BIOS_MEMREGION_PADDR 0xf0000
#define BIOS_MEMREGION_SIZE      0x10000

#define X86_EBDA_MEMREGION_PADDR 0x80000
#define EBDA_MEMREGION_SIZE      0x20000

#define X86_VIDEO_DEVICE_PADDR 0xb8000

typedef struct
{
    reg_t ip, cs;
    reg_t eflags;
    reg_t sp, ss;
} __packed x86_iret_params_t;

typedef struct
{
    reg_t ds, es, fs, gs;
#if MOS_BITS == 64
    reg_t r8, r9, r10, r11, r12, r13, r14, r15;
#endif
    reg_t di, si, bp, _sp, bx, dx, cx, ax; // the _esp is unused, see iret_params.esp
    reg32_t interrupt_number, error_code;
    x86_iret_params_t iret_params;
} __packed x86_stack_frame;

#if MOS_BITS == 64
MOS_STATIC_ASSERT(sizeof(x86_stack_frame) == 208, "x86_stack_frame has incorrect size");
#else
MOS_STATIC_ASSERT(sizeof(x86_stack_frame) == 76, "x86_stack_frame has incorrect size");
#endif

// defined in the linker script 'multiboot.ld'
extern const char __MOS_KERNEL_CODE_START[], __MOS_KERNEL_CODE_END[];     // Kernel text
extern const char __MOS_KERNEL_RODATA_START[], __MOS_KERNEL_RODATA_END[]; // Kernel rodata
extern const char __MOS_KERNEL_RW_START[], __MOS_KERNEL_RW_END[];         // Kernel read-write data
extern const char __MOS_KERNEL_END[];                                     // Kernel end

extern mos_platform_info_t x86_platform;
extern serial_console_t com1_console;

void x86_start_kernel();
