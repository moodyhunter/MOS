// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/platform/platform.h>
#include <mos/x86/boot/multiboot.h>

#define X86_BIOS_MEMREGION_PADDR 0xf0000
#define BIOS_MEMREGION_SIZE      0x10000

#define X86_EBDA_MEMREGION_PADDR 0x80000
#define EBDA_MEMREGION_SIZE      0x20000

#define X86_VIDEO_DEVICE_PADDR 0xb8000

typedef struct
{
    reg32_t eip, cs;
    reg32_t eflags;
    reg32_t esp, ss;
} __packed x86_iret_params_t;

typedef struct
{
    reg32_t ds, es, fs, gs;
    reg32_t edi, esi, ebp, _esp, ebx, edx, ecx, eax; // the _esp is unused, see iret_params.esp
    reg32_t interrupt_number, error_code;
    x86_iret_params_t iret_params;
} __packed x86_stack_frame;

MOS_STATIC_ASSERT(sizeof(x86_stack_frame) == 76, "x86_stack_frame has incorrect size");

typedef struct
{
    u32 mb_magic;
    multiboot_info_t *mb_info;
} __packed x86_startup_info;

// defined in the linker script 'multiboot.ld'
extern const char __MOS_KERNEL_CODE_START[], __MOS_KERNEL_CODE_END[];     // Kernel text
extern const char __MOS_KERNEL_RODATA_START[], __MOS_KERNEL_RODATA_END[]; // Kernel rodata
extern const char __MOS_KERNEL_RW_START[], __MOS_KERNEL_RW_END[];         // Kernel read-write data
extern const char __MOS_KERNEL_END[];                                     // Kernel end

extern mos_platform_info_t x86_platform;
extern bool x86_initrd_present;
