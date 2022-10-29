// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/platform/platform.h"
#include "mos/x86/boot/multiboot.h"
#include "mos/x86/gdt/gdt_types.h"
#include "mos/x86/interrupt/idt_types.h"
#include "mos/x86/tasks/tss_types.h"

static_assert(sizeof(void *) == 4, "x86_64 is not supported");

#define GDT_SEGMENT_NULL     0x00
#define GDT_SEGMENT_KCODE    0x08
#define GDT_SEGMENT_KDATA    0x10
#define GDT_SEGMENT_USERCODE 0x18
#define GDT_SEGMENT_USERDATA 0x20
#define GDT_SEGMENT_TSS      0x28
#define GDT_ENTRY_COUNT      6

#define X86_MAX_MEM_SIZE ((u32) (4 GB - 1))

#define X86_BIOS_MEMREGION_PADDR 0xf0000
#define BIOS_MEMREGION_SIZE      0x10000

#define X86_EBDA_MEMREGION_PADDR 0x80000
#define EBDA_MEMREGION_SIZE      0x20000

#define X86_VIDEO_DEVICE_PADDR 0xb8000

typedef struct
{
    reg32_t eip, cs;
    reg32_t eflags;
    reg32_t ss, esp;
} __packed x86_iret_params_t;

typedef struct
{
    reg32_t ds, es, fs, gs;
    reg32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    reg32_t interrupt_number, error_code;
    x86_iret_params_t intrrupt;
} __packed x86_stack_frame;

static_assert(sizeof(x86_stack_frame) == 76, "x86_stack_frame is not 76 bytes");

typedef struct
{
    u32 mb_magic;
    multiboot_info_t *mb_info;
    size_t initrd_size;
    uintptr_t bios_region_start;
} __packed x86_startup_info;

// defined in the linker script 'multiboot.ld'
extern const char __MOS_KERNEL_CODE_START, __MOS_KERNEL_CODE_END;     // Kernel text
extern const char __MOS_KERNEL_RODATA_START, __MOS_KERNEL_RODATA_END; // Kernel rodata
extern const char __MOS_KERNEL_RW_START;                              // Kernel read-write data {
extern const char __MOS_X86_PAGING_AREA_START;                        //     Paging area {
extern const char __MOS_X86_PAGING_AREA_END;                          //     }
extern const char __MOS_KERNEL_RW_END;                                // }
extern const char __MOS_KERNEL_END;                                   // End of kernel

extern const uintptr_t mos_kernel_end;
extern mos_platform_t x86_platform;
extern tss32_t tss_entry;

void x86_gdt_init();
void x86_ap_gdt_init();
void x86_idt_init();
void x86_tss_init();

// The following 5 symbols are defined in the descriptor_flush.asm file.
extern asmlinkage void gdt32_flush(gdt_ptr32_t *gdt_ptr);
extern asmlinkage void idt32_flush(idtr32_t *idtr);
extern asmlinkage void tss32_flush(u32 tss_selector);
extern asmlinkage void gdt32_flush_only(gdt_ptr32_t *gdt_ptr);
