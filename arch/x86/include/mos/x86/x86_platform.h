// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mos_global.h"
#include "mos/types.h"
#include "mos/x86/init/gdt_types.h"
#include "mos/x86/init/idt_types.h"
#include "mos/x86/x86_interrupt.h"

static_assert(sizeof(void *) == 4, "x86_64 is not supported");

// Number of gdt entries
#define GDT_ENTRY_COUNT 4

#define GDT_SEGMENT_NULL  0x00
#define GDT_SEGMENT_KCODE 0x08
#define GDT_SEGMENT_KDATA 0x10
#define GDT_SEGMENT_UCODE 0x18

#define PIC1         0x20 // IO base address for master PIC
#define PIC2         0xA0 // IO base address for slave  PIC
#define PIC1_COMMAND (PIC1)
#define PIC1_DATA    (PIC1 + 1)
#define PIC2_COMMAND (PIC2)
#define PIC2_DATA    (PIC2 + 1)

#define X86_PAGE_SIZE      (4 KB)
#define X86_MAX_MEM_SIZE   ((u32) 4 GB - 1)
#define X86_MAX_KHEAP_SIZE (X86_MAX_ADDR - MOS_X86_HEAP_BASE_VADDR)

#define X86_ALIGN_UP_TO_PAGE(addr)   (((addr) + X86_PAGE_SIZE - 1) & ~(X86_PAGE_SIZE - 1))
#define X86_ALIGN_DOWN_TO_PAGE(addr) ((addr) & ~(X86_PAGE_SIZE - 1))

typedef struct
{
    u32 edi, esi, ebp, esp, ebx, edx, ecx, eax;
    u32 ds, es, fs, gs;
    u32 interrupt_number;
    u32 error_code;
    u32 eip, cs, eflags;
} __packed x86_stack_frame;

static_assert(sizeof(x86_stack_frame) == 68, "x86_stack_frame is not 68 bytes");

// defined in the linker script 'multiboot.ld'
extern const char __MOS_SECTION_KERNEL_START;                                 // Kernel ELF {
extern const char __MOS_SECTION_MULTIBOOT_START, __MOS_SECTION_MULTIBOOT_END; //   Multiboot Code / Data
extern const char __MOS_KERNEL_RO_START;                                      //   Kernel read-only data {
extern const char __MOS_KERNEL_TEXT_START, __MOS_KERNEL_TEXT_END;             //     Kernel text
extern const char __MOS_KERNEL_RODATA_START, __MOS_KERNEL_RODATA_END;         //     Kernel rodata
extern const char __MOS_KERNEL_RO_END;                                        //   }
extern const char __MOS_X86_PAGING_AREA_START;                                //   Paging area {
extern const char __MOS_X86_PAGE_TABLE_START;                                 //     Page table
extern const char __MOS_X86_PAGING_AREA_END;                                  //   }
extern const char __MOS_SECTION_KERNEL_END;                                   // }

extern const uintptr_t x86_kernel_start;
extern const uintptr_t x86_kernel_end;

extern mos_platform_cpu_info_t x86_cpu_info;

void x86_gdt_init();
void x86_ap_gdt_init();
void x86_idt_init();

// The following 3 symbols are defined in the descriptor_flush.asm file.
extern void gdt32_flush(gdt_ptr32_t *gdt_ptr);
extern void idt32_flush(idtr32_t *idtr);
extern void gdt32_flush_only(gdt_ptr32_t *gdt_ptr);
