// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/kallsyms.h"
#include "mos/ksyscall_entry.h"
#include "mos/tasks/task_types.h"

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
        {
            ptr_t drx[6]; // DR0, DR1, DR2, DR3 and DR6, DR7
            __asm__ volatile("mov %%dr0, %0\n"
                             "mov %%dr1, %1\n"
                             "mov %%dr2, %2\n"
                             "mov %%dr3, %3\n"
                             "mov %%dr6, %4\n"
                             "mov %%dr7, %5\n"
                             : "=r"(drx[0]), "=r"(drx[1]), "=r"(drx[2]), "=r"(drx[3]), "=r"(drx[4]), "=r"(drx[5]));

            pr_emerg("cpu %d: %s (%lu) at " PTR_FMT " (DR0: " PTR_FMT " DR1: " PTR_FMT " DR2: " PTR_FMT " DR3: " PTR_FMT " DR6: " PTR_FMT " DR7: " PTR_FMT ")",
                     lapic_get_id(), name, stack->interrupt_number, stack->iret_params.ip, drx[0], drx[1], drx[2], drx[3], drx[4], drx[5]);

            return;
        }
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
                    pr_emph("%s page fault: thread %pt, process %pp at " PTR_FMT ", instruction " PTR_FMT, //
                            is_user ? "user" : "kernel",                                                   //
                            (void *) current,                                                              //
                            (void *) current->owner,                                                       //
                            fault_address,                                                                 //
                            (ptr_t) stack->iret_params.ip                                                  //
                    );
                }

                if (is_write && is_exec)
                    mos_panic("Cannot write and execute at the same time");

                const pagefault_info_t info = {
                    .op_write = is_write,
                    .page_present = present,
                    .userfault = is_user,
                };

                bool result = mm_handle_fault(fault_address, &info);

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
            pr_emerg("  thread: %pt", (void *) current);
            pr_emerg("  process: %pp", current ? (void *) current->owner : NULL);

            if (fault_address < 1 KB)
                pr_emerg("  possible null pointer dereference");

            if (is_user && fault_address > MOS_KERNEL_START_VADDR)
                pr_emerg("  kernel address dereference");

            if (stack->iret_params.ip > MOS_KERNEL_START_VADDR)
                pr_emerg("  in kernel function %ps", (void *) stack->iret_params.ip);

            pr_emerg("  CR3: " PTR_FMT, x86_get_cr3() + platform_info->direct_map_base);

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
    frame->ax = ksyscall_enter(frame->ax, frame->bx, frame->cx, frame->dx, frame->si, frame->di, frame->bp);

    MOS_ASSERT_X(current_thread->state == THREAD_STATE_RUNNING, "thread %pt is not in 'running' state", (void *) current_thread);

    // flags may have been changed by platform_arch_syscall
    x86_process_options_t *options = current_process->platform_options;
    if (options)
    {
        if (options->iopl_enabled)
            frame->iret_params.eflags |= 0x3000; // enable IOPL
        else
            frame->iret_params.eflags &= ~0x3000; // disable IOPL
    }
}

void x86_handle_interrupt(ptr_t rsp)
{
    x86_stack_frame *frame = (x86_stack_frame *) rsp;

    if (likely(current_thread))
    {
        x86_thread_context_t *context = container_of(current_thread->context, x86_thread_context_t, inner);
        context->regs = *frame;
        context->inner.instruction = frame->iret_params.ip;
        context->inner.stack = frame->iret_params.sp;
    }

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
