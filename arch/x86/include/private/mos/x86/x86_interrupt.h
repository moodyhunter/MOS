// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/lib/structures/list.h>
#include <mos/mos_global.h>
#include <mos/types.h>

#define IRQ_BASE 0x20
#define IPI_BASE 0x50

#define ISR_MAX_COUNT   32
#define IRQ_MAX_COUNT   16
#define IDT_ENTRY_COUNT 256

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

MOS_STATIC_ASSERT(IRQ_BASE > EXCEPTION_MAX, "IRQ_BASE is too small, possibly overlapping with exceptions");

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

MOS_STATIC_ASSERT(IRQ_MAX_COUNT == IRQ_MAX, "IRQ_MAX_COUNT is not equal to IRQ_MAX");

void pic_remap_irq(void);

// The following `extern` symbols are defined in the interrupt_handler.asm file.
extern list_head irq_handlers[IRQ_MAX_COUNT];
extern void *isr_stub_table[];
extern void *irq_stub_table[];

void x86_irq_handler_init(void);
void x86_handle_interrupt(ptr_t esp);

bool x86_install_interrupt_handler(u32 irq, void (*handler)(u32 irq));
