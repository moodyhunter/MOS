// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/kallsyms.h"

#include <mos/interrupt/ipi.h>
#include <mos/lib/structures/list.h>
#include <mos/mm/cow.h>
#include <mos/platform/platform.h>
#include <mos/printk.h>
#include <mos/syscall/dispatcher.h>
#include <mos/x86/cpu/cpu.h>
#include <mos/x86/devices/port.h>
#include <mos/x86/interrupt/apic.h>
#include <mos/x86/tasks/context.h>
#include <mos/x86/x86_interrupt.h>
#include <mos/x86/x86_platform.h>
#include <stdlib.h>

static const char *const x86_exception_names[EXCEPTION_COUNT] = {
    "Divide-By-Zero Error",
    "Debug",
    "Non-Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 Floating-Point Error",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Hypervisor Injection Exception",
    "VMM Communication Exception",
    "Security Exception",
    "Reserved",
};

typedef struct
{
    as_linked_list;
    void (*handler)(u32 irq);
} x86_irq_handler_t;

list_head irq_handlers[IRQ_MAX_COUNT];

void x86_irq_handler_init(void)
{
    for (int i = 0; i < IRQ_MAX_COUNT; i++)
        linked_list_init(&irq_handlers[i]);
}

bool x86_install_interrupt_handler(u32 irq, void (*handler)(u32 irq))
{
    MOS_ASSERT(irq < IRQ_MAX_COUNT);
    x86_irq_handler_t *desc = kzalloc(sizeof(x86_irq_handler_t));
    linked_list_init(list_node(desc));
    desc->handler = handler;
    list_node_append(&irq_handlers[irq], list_node(desc));
    return true;
}

void x86_dump_registers(x86_stack_frame *frame)
{
    pr_emph("General Purpose Registers:\n"
            "  RAX: " PTR_FMT " RBX: " PTR_FMT " RCX: " PTR_FMT " RDX: " PTR_FMT "\n"
            "  RSI: " PTR_FMT " RDI: " PTR_FMT " RBP: " PTR_FMT " RSP: " PTR_FMT "\n"
            "  R8:  " PTR_FMT " R9:  " PTR_FMT " R10: " PTR_FMT " R11: " PTR_FMT "\n"
            "  R12: " PTR_FMT " R13: " PTR_FMT " R14: " PTR_FMT " R15: " PTR_FMT "\n"
            "  IP:  " PTR_FMT "\n"
            "Segment Registers:\n"
            "  FS:  0x%lx GS:  0x%lx\n"
            "Context:\n"
            "  EFLAGS:       " PTR_FMT "\n"
            "  Instruction:  0x%lx:" PTR_FMT "\n"
            "  Stack:        0x%lx:" PTR_FMT,
            frame->ax, frame->bx, frame->cx, frame->dx,             //
            frame->si, frame->di, frame->bp, frame->iret_params.sp, //
            frame->r8, frame->r9, frame->r10, frame->r11,           //
            frame->r12, frame->r13, frame->r14, frame->r15,         //
            frame->iret_params.ip,                                  //
            frame->fs, frame->gs,                                   //
            frame->iret_params.eflags,                              //
            frame->iret_params.cs, frame->iret_params.ip,           //
            frame->iret_params.ss, frame->iret_params.sp            //
    );
}

static void x86_handle_nmi(x86_stack_frame *frame)
{
    pr_emph("cpu %d: NMI received", lapic_get_id());

    u8 scp1 = port_inb(0x92);
    u8 scp2 = port_inb(0x61);

    static const char *const scp1_names[] = { "Alternate Hot Reset", "Alternate A20 Gate", "[RESERVED]",     "Security Lock",
                                              "Watchdog Timer",      "[RESERVED]",         "HDD 2 Activity", "HDD 1 Activity" };

    static const char *const scp2_names[] = { "Timer 2 Tied to Speaker", "Speaker Data Enable", "Parity Check Enable", "Channel Check Enable",
                                              "Refresh Request",         "Timer 2 Output",      "Channel Check",       "Parity Check" };

    for (int bit = 0; bit < 8; bit++)
        if (scp1 & (1 << bit))
            pr_emph("  %s", scp1_names[bit]);

    for (int bit = 0; bit < 8; bit++)
        if (scp2 & (1 << bit))
            pr_emph("  %s", scp2_names[bit]);

    x86_dump_registers(frame);
    mos_panic("NMI received");
}

static void x86_handle_exception(x86_stack_frame *stack)
{
    MOS_ASSERT(stack->interrupt_number < EXCEPTION_COUNT);

    const char *name = x86_exception_names[stack->interrupt_number];
    const char *intr_type = "";

    // Faults: These can be corrected and the program may continue as if nothing happened.
    // Traps:  Traps are reported immediately after the execution of the trapping instruction.
    // Aborts: Some severe unrecoverable error.
    switch ((x86_exception_enum_t) stack->interrupt_number)
    {
        case EXCEPTION_NMI:
        {
            x86_handle_nmi(stack);
            return;
        }
        case EXCEPTION_DIVIDE_ERROR:
        case EXCEPTION_DEBUG:
        case EXCEPTION_OVERFLOW:
        case EXCEPTION_BOUND_RANGE_EXCEEDED:
        case EXCEPTION_INVALID_OPCODE:
        case EXCEPTION_DEVICE_NOT_AVAILABLE:
        case EXCEPTION_COPROCESSOR_SEGMENT_OVERRUN:
        case EXCEPTION_INVALID_TSS:
        case EXCEPTION_SEGMENT_NOT_PRESENT:
        case EXCEPTION_STACK_SEGMENT_FAULT:
        case EXCEPTION_GENERAL_PROTECTION_FAULT:
        case EXCEPTION_FPU_ERROR:
        case EXCEPTION_ALIGNMENT_CHECK:
        case EXCEPTION_SIMD_ERROR:
        case EXCEPTION_VIRTUALIZATION_EXCEPTION:
        case EXCEPTION_CONTROL_PROTECTION_EXCEPTION:
        case EXCEPTION_HYPERVISOR_EXCEPTION:
        case EXCEPTION_VMM_COMMUNICATION_EXCEPTION:
        case EXCEPTION_SECURITY_EXCEPTION:
        {
            intr_type = "fault";
            break;
        }

        case EXCEPTION_BREAKPOINT:
        {
            mos_warn("Breakpoint not handled.");
            return;
        }

        case EXCEPTION_PAGE_FAULT:
        {
            intr_type = "page fault";

            ptr_t fault_address;
            __asm__ volatile("mov %%cr2, %0" : "=r"(fault_address));

            bool present = (stack->error_code & 0x1) != 0;
            bool is_write = (stack->error_code & 0x2) != 0;
            bool is_user = (stack->error_code & 0x4) != 0;
            bool is_exec = (stack->error_code & 0x10) != 0;

            thread_t *current = current_thread;

            if (current)
            {
                if (MOS_DEBUG_FEATURE(cow))
                {
                    pr_emph("%s page fault: thread %d (%s), process %d (%s) at " PTR_FMT ", instruction " PTR_FMT, //
                            is_user ? "user" : "kernel",                                                           //
                            current->tid,                                                                          //
                            current->name,                                                                         //
                            current->owner->pid,                                                                   //
                            current->owner->name,                                                                  //
                            fault_address,                                                                         //
                            (ptr_t) stack->iret_params.ip                                                          //
                    );
                }

                if (is_write && is_exec)
                    mos_panic("Cannot write and execute at the same time");

                bool result = mm_handle_pgfault(fault_address, present, is_write, is_user, is_exec);

                if (result)
                    return;
            }
            else
            {
                // early boot page fault?
                mos_warn("early boot page fault");
            }

            pr_emerg("Unhandled Page Fault");
            pr_emerg("  %s mode invalid %s page %s%s at [" PTR_FMT "]", //
                     is_user ? "User" : "Kernel",                       //
                     present ? "present" : "non-present",               //
                     is_write ? "write" : "read",                       //
                     is_exec ? " (NX violation)" : "",                  //
                     fault_address                                      //
            );
            pr_emerg("  instruction: " PTR_FMT, (ptr_t) stack->iret_params.ip);
            pr_emerg("  thread: %d (%s)", current->tid, current->name);
            pr_emerg("  process: %d (%s)", current->owner->pid, current->owner->name);

            if (fault_address < 1 KB)
                pr_emerg("  possible null pointer dereference");

            if (is_user && fault_address > MOS_KERNEL_START_VADDR)
                pr_emerg("  kernel address dereference");

            if (stack->iret_params.ip > platform_info->k_code.vaddr)
                pr_emerg("  in kernel function '%s'", kallsyms_get_symbol_name(stack->iret_params.ip));

            pr_emerg("  CR3: " PTR_FMT, (ptr_t) x86_get_cr3() + platform_info->direct_map_base);

            x86_dump_registers(stack);
            mos_panic("Unable to recover from page fault.");
            MOS_UNREACHABLE();
        }

        case EXCEPTION_DOUBLE_FAULT:
        case EXCEPTION_MACHINE_CHECK:
        {
            intr_type = "abort";
            break;
        }
        default:
        {
            name = "unknown";
            intr_type = "unknown";
        }
    }

    x86_dump_registers(stack);
    mos_panic("x86 %s:\nInterrupt #%lu ('%s', error code %lu)", intr_type, stack->interrupt_number, name, stack->error_code);
}

static void x86_handle_irq(x86_stack_frame *frame)
{
    int irq = frame->interrupt_number - IRQ_BASE;

    lapic_eoi();

    bool irq_handled = false;
    list_foreach(x86_irq_handler_t, handler, irq_handlers[irq])
    {
        irq_handled = true;
        handler->handler(irq);
    }

    if (unlikely(!irq_handled))
        pr_warn("IRQ %d not handled!", irq);
}

static void x86_handle_syscall(x86_stack_frame *frame)
{
    thread_t *current = current_thread;

    if (likely(current))
    {
        x86_thread_context_t *context = container_of(current->context, x86_thread_context_t, inner);
        context->regs = *frame;
        context->inner.instruction = frame->iret_params.ip;
        context->inner.stack = frame->iret_params.sp;
    }

    frame->ax = dispatch_syscall(frame->ax, frame->bx, frame->cx, frame->dx, frame->si, frame->di, frame->bp);

    if (likely(current))
    {
        MOS_ASSERT_X(current->state == THREAD_STATE_RUNNING, "thread %d is not in 'running' state", current->tid);

        // flags may have been changed by platform_arch_syscall
        x86_process_options_t *options = current->owner->platform_options;
        if (options)
        {
            if (options->iopl_enabled)
                frame->iret_params.eflags |= 0x3000; // enable IOPL
            else
                frame->iret_params.eflags &= ~0x3000; // disable IOPL
        }
    }

    // frame->iret_params.eflags |= 0x200; // enable interrupts
}

void x86_handle_interrupt(ptr_t rsp)
{
    x86_stack_frame *frame = (x86_stack_frame *) rsp;
    if (frame->interrupt_number < IRQ_BASE)
        x86_handle_exception(frame);
    else if (frame->interrupt_number >= IRQ_BASE && frame->interrupt_number < IRQ_BASE + IRQ_MAX)
        x86_handle_irq(frame);
    else if (frame->interrupt_number >= IPI_BASE && frame->interrupt_number < IPI_BASE + IPI_TYPE_MAX)
        ipi_do_handle((ipi_type_t) (frame->interrupt_number - IPI_BASE));
    else if (frame->interrupt_number == MOS_SYSCALL_INTR)
        x86_handle_syscall(frame);
    else
        pr_warn("Unknown interrupt number: %lu", frame->interrupt_number);
}
