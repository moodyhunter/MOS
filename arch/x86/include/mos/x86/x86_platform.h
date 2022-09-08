// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/platform/platform.h"
#include "mos/x86/gdt/gdt_types.h"
#include "mos/x86/interrupt/idt_types.h"

static_assert(sizeof(void *) == 4, "x86_64 is not supported");

#define GDT_SEGMENT_NULL     0x00
#define GDT_SEGMENT_KCODE    0x08
#define GDT_SEGMENT_KDATA    0x10
#define GDT_SEGMENT_USERCODE 0x18
#define GDT_SEGMENT_USERDATA 0x20
#define GDT_SEGMENT_TSS      0x28

#define GDT_ENTRY_COUNT 6 // Number of gdt entries

#define X86_PAGE_SIZE    (4 KB)
#define X86_MAX_MEM_SIZE ((u32) (4 GB - 1))

#define X86_ALIGN_UP_TO_PAGE(addr)   (((addr) + X86_PAGE_SIZE - 1) & ~(X86_PAGE_SIZE - 1))
#define X86_ALIGN_DOWN_TO_PAGE(addr) ((addr) & ~(X86_PAGE_SIZE - 1))

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

static_assert(sizeof(x86_stack_frame) == 76, "x86_stack_frame is not 68 bytes");

// defined in the linker script 'multiboot.ld'
extern const char __MOS_SECTION_MULTIBOOT_START, __MOS_SECTION_MULTIBOOT_END; // Multiboot Code / Data
extern const char __MOS_KERNEL_RO_START;                                      //     Kernel read-only data {
extern const char __MOS_KERNEL_TEXT_START, __MOS_KERNEL_TEXT_END;             //     Kernel text
extern const char __MOS_KERNEL_RODATA_START, __MOS_KERNEL_RODATA_END;         //     Kernel rodata
extern const char __MOS_KERNEL_RO_END;                                        // }
extern const char __MOS_KERNEL_RW_START;                                      // Kernel read-write data {
extern const char __MOS_X86_PAGING_AREA_START;                                //     Paging area {
extern const char __MOS_X86_PAGING_AREA_END;                                  //     }
extern const char __MOS_KERNEL_RW_END;                                        // }
extern const char __MOS_KERNEL_END;                                           // End of kernel

extern const uintptr_t mos_kernel_end;
extern mos_platform_t x86_platform;

void x86_gdt_init();
void x86_ap_gdt_init();
void x86_idt_init();
void x86_tss_init();

// The following 5 symbols are defined in the descriptor_flush.asm file.
extern asmlinkage void gdt32_flush(gdt_ptr32_t *gdt_ptr);
extern asmlinkage void idt32_flush(idtr32_t *idtr);
extern asmlinkage void tss32_flush(u32 tss_selector);
extern asmlinkage void gdt32_flush_only(gdt_ptr32_t *gdt_ptr);
