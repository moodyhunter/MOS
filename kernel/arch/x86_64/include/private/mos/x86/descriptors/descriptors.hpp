// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/platform/platform.hpp>
#include <mos/types.hpp>

#define GDT_NULL    0x00
#define GDT_SEGMENT 0x10
#define GDT_PRESENT 0x80

#define GDT_GRANULARITY_BYTE 0x40
#define GDT_GRANULARITY_PAGE 0xC0

#define GDT_SEGMENT_NULL 0x00

#define GDT_SEGMENT_KCODE    0x10
#define GDT_SEGMENT_KDATA    0x20
#define GDT_SEGMENT_USERCODE 0x30
#define GDT_SEGMENT_USERDATA 0x40
#define GDT_SEGMENT_TSS      0x50

#define GDT_ENTRY_COUNT 6

typedef struct
{
    u32 limit_low : 16;             //
    u32 base_low : 24;              //
    u32 accessed : 1;               //
    u32 read_write : 1;             // readable for code, writable for data
    u32 conforming_expand_down : 1; // conforming for code, expand down for data
    u32 executable : 1;             // 1 for code, 0 for data
    u32 code_data_segment : 1;      // should be 1 for everything but TSS and LDT
    u32 dpl : 2;                    // privilege level
    u32 present : 1;                //
    u32 limit_high : 4;             //
    u32 available : 1;              // only used in software; has no effect on hardware
    u32 long_mode_code : 1;         // long-mode code segment
    u32 pm32_segment : 1;           // 32-bit opcodes for code, uint32_t stack for data
    u32 granularity : 1;            // 1 to use 4k page addressing, 0 for byte addressing
    u32 base_high : 8;
    u32 base_veryhigh; // upper 32 bits of base address
    u32 reserved;
} __packed gdt_entry_t;

MOS_STATIC_ASSERT(sizeof(gdt_entry_t) == 16, "gdt_entry_t is not 16 bytes");

typedef struct
{
    u16 limit;
    gdt_entry_t *base;
} __packed gdt_ptr_t;

MOS_STATIC_ASSERT(sizeof(gdt_ptr_t) == 2 + sizeof(void *), "gdt_ptr_t is not 6 bytes");

typedef struct
{
    u32 __reserved1;
    u64 rspN[3];
    u64 __reserved2;
    u64 IntrStackTable[7];
    u64 __reserved3;
    u16 __reserved4;
    u16 iomap;
} __packed tss64_t;

MOS_STATIC_ASSERT(sizeof(tss64_t) == 0x68, "tss64_t is not 0x68 bytes");

typedef struct
{
    tss64_t tss __aligned(32);
    gdt_entry_t gdt[GDT_ENTRY_COUNT] __aligned(32);
    gdt_ptr_t gdt_ptr __aligned(32);
} __packed x86_cpu_descriptor_t;

extern PER_CPU_DECLARE(x86_cpu_descriptor_t, x86_cpu_descriptor);

typedef struct
{
    u16 isr_low; // The lower 16 bits of the ISR's address
    u16 segment; // The GDT segment selector that the CPU will load into CS before calling the ISR
    u32 reserved : 8;
    u32 type : 4; // The type of interrupt
    u32 zero : 1;
    u32 dpl : 2;
    u32 present : 1;
    u32 isr_high : 16; // The upper 16 bits of the ISR's address
    u32 isr_veryhigh;  // The upper 32 bits of the ISR's address
    u32 reserved2;
} __packed idt_entry_t;

typedef struct
{
    u16 limit;
    idt_entry_t *base;
} __packed idtr_t;

MOS_STATIC_ASSERT(sizeof(idt_entry_t) == 16, "idt_entry_t is not 16 bytes");

MOS_STATIC_ASSERT(sizeof(idtr_t) == 2 + sizeof(void *), "idtr32_t is not 6 bytes");

void x86_init_percpu_gdt(void);
void x86_init_percpu_tss(void);
void x86_init_percpu_idt(void);

void x86_idt_init(void);

// The following 5 symbols are defined in the descriptor_flush.asm file.
extern "C" void gdt_flush(gdt_ptr_t *gdt_ptr);
extern "C" void tss_flush(u32 tss_selector);
extern "C" void gdt_flush_only(gdt_ptr_t *gdt_ptr);
