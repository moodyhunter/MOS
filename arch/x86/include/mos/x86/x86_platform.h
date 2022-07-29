// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mos_global.h"
#include "mos/types.h"

static_assert(sizeof(void *) == 4, "x86_64 is not supported");

#define IRQ_BASE    0x20
#define IRQ_SYSCALL 0x80

#define ISR_MAX_COUNT   32
#define IRQ_MAX_COUNT   16
#define IDT_ENTRY_COUNT 256

// Number of gdt entries
#define GDT_TABLE_SIZE 6

#define GDT_SEGMENT_NULL     0x00
#define GDT_SEGMENT_KCODE    0x08
#define GDT_SEGMENT_KDATA    0x10
#define GDT_SEGMENT_USERCODE 0x18
#define GDT_SEGMENT_USERDATA 0x20
#define GDT_SEGMENT_TSS      0x28

#define PIC1         0x20 // IO base address for master PIC
#define PIC2         0xA0 // IO base address for slave  PIC
#define PIC1_COMMAND (PIC1)
#define PIC1_DATA    (PIC1 + 1)
#define PIC2_COMMAND (PIC2)
#define PIC2_DATA    (PIC2 + 1)

typedef enum
{
    EXCEPTION_DIVIDE_ERROR = 0,
    EXCEPTION_DEBUG = 1,
    EXCEPTION_NMI = 2,
    EXCEPTION_BREAKPOINT = 3,
    EXCEPTION_OVERFLOW = 4,
    EXCEPTION_BOUND_RANGE_EXCEEDED = 5,
    EXCEPTION_INVALID_OPCODE = 6,
    EXCEPTION_DEVICE_NOT_AVAILABLE = 7,
    EXCEPTION_DOUBLE_FAULT = 8,
    EXCEPTION_COPROCESSOR_SEGMENT_OVERRUN = 9,
    EXCEPTION_INVALID_TSS = 10,
    EXCEPTION_SEGMENT_NOT_PRESENT = 11,
    EXCEPTION_STACK_SEGMENT_FAULT = 12,
    EXCEPTION_GENERAL_PROTECTION_FAULT = 13,
    EXCEPTION_PAGE_FAULT = 14,
    // 15 is reserved
    EXCEPTION_FPU_ERROR = 16,
    EXCEPTION_ALIGNMENT_CHECK = 17,
    EXCEPTION_MACHINE_CHECK = 18,
    EXCEPTION_SIMD_ERROR = 19,
    EXCEPTION_VIRTUALIZATION_EXCEPTION = 20,
    EXCEPTION_CONTROL_PROTECTION_EXCEPTION = 21,
    // 22-27 are reserved
    EXCEPTION_HYPERVISOR_EXCEPTION = 28,
    EXCEPTION_VMM_COMMUNICATION_EXCEPTION = 29,
    EXCEPTION_SECURITY_EXCEPTION = 30,
    // 31 is reserved
    EXCEPTION_MAX = 31,
    EXCEPTION_COUNT = 32
} x86_exception_enum_t;

static_assert(IRQ_BASE > EXCEPTION_MAX, "IRQ_BASE is too small, possibly overlapping with exceptions");

typedef enum
{
    IRQ_TIMER = 0,
    IRQ_KEYBOARD = 1,
    IRQ_CASCADE = 2,
    IRQ_COM2 = 3,
    IRQ_COM1 = 4,
    IRQ_LPT2 = 5,
    IRQ_FLOPPY = 6,
    IRQ_LPT1 = 7,
    IRQ_CMOS_RTC = 8,
    IRQ_FREE1 = 9,
    IRQ_FREE2 = 10,
    IRQ_FREE3 = 11,
    IRQ_PS2_MOUSE = 12,
    IRQ_FPU = 13,
    IRQ_ATA_PRIMARY = 14,
    IRQ_ATA_SECONDARY = 15,
    IRQ_MAX = 16,
} x86_irq_enum_t;

typedef struct
{
    u32 edi, esi, ebp, esp, ebx, edx, ecx, eax;
    u32 ds, es, fs, gs;
    u32 interrupt_number;
    u32 err_code;
    u32 eip, cs, eflags;
} __attr_packed x86_stack_frame;

static_assert(sizeof(x86_stack_frame) == 68, "x86_stack_frame is not 68 bytes");

void x86_handle_interrupt(u32 esp);
