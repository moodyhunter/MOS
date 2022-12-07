// SPDX-License-Identifier: GPL-3.0-or-later
#include "mos/x86/x86_interrupt.h"

#include "lib/structures/list.h"
#include "mos/mm/cow.h"
#include "mos/mm/kmalloc.h"
#include "mos/mos_global.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/syscall/dispatcher.h"
#include "mos/tasks/task_type.h"
#include "mos/x86/devices/port.h"
#include "mos/x86/interrupt/pic.h"
#include "mos/x86/tasks/context.h"
#include "mos/x86/x86_platform.h"

static const char *x86_exception_names[EXCEPTION_COUNT] = {
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

static void x86_handle_irq(x86_stack_frame *frame);
static void x86_handle_exception(x86_stack_frame *frame);
list_node_t irq_handlers[IRQ_MAX_COUNT];

void x86_irq_handler_init(void)
{
    for (int i = 0; i < IRQ_MAX_COUNT; i++)
        linked_list_init(&irq_handlers[i]);
}

void x86_disable_interrupts()
{
    __asm__ volatile("cli");
}

void x86_enable_interrupts()
{
    __asm__ volatile("sti");
}

bool x86_install_interrupt_handler(u32 irq, void (*handler)(u32 irq))
{
    x86_irq_handler_t *desc = kmalloc(sizeof(x86_irq_handler_t));
    desc->handler = handler;
    list_node_append(&irq_handlers[irq], list_node(desc));
    return true;
}

static void x86_dump_registers(x86_stack_frame *frame)
{
    pr_emph("General Purpose Registers:\n"
            "  EAX: 0x%08x EBX: 0x%08x ECX: 0x%08x EDX: 0x%08x\n"
            "  ESI: 0x%08x EDI: 0x%08x EBP: 0x%08x ESP: 0x%08x\n"
            "  EIP: 0x%08x\n"
            "Segment Registers:\n"
            "  DS:  0x%08x ES:  0x%08x FS:  0x%08x GS:  0x%08x\n"
            "Context:\n"
            "  EFLAGS:       0x%08x\n"
            "  Instruction:  0x%x:%08x\n"
            "  Stack:        0x%x:%08x",
            frame->eax, frame->ebx, frame->ecx, frame->edx, //
            frame->esi, frame->edi, frame->ebp, frame->esp, //
            frame->iret_params.eip,                         //
            frame->ds, frame->es, frame->fs, frame->gs,     //
            frame->iret_params.eflags,                      //
            frame->iret_params.cs, frame->iret_params.eip,  //
            frame->iret_params.ss, frame->iret_params.esp   //
    );
}

void x86_handle_interrupt(u32 esp)
{
    x86_stack_frame *frame = (x86_stack_frame *) esp;

    thread_t *current = current_thread;
    if (likely(current))
    {
        current->stack.head = frame->iret_params.esp;
        x86_thread_context_t *context = container_of(current->context, x86_thread_context_t, inner);
        context->ebp = frame->ebp;
        context->inner.instruction = frame->iret_params.eip;
    }

    if (frame->interrupt_number < IRQ_BASE)
        x86_handle_exception(frame);
    else if (frame->interrupt_number < MOS_SYSCALL_INTR)
        x86_handle_irq(frame);
    else if (frame->interrupt_number == MOS_SYSCALL_INTR)
    {
#pragma message "TODO: Implement syscall handling for other arguments"
        frame->eax = (reg32_t) dispatch_syscall(frame->eax, frame->ebx, frame->ecx, frame->edx, frame->esi, frame->edi, 0, 0, 0);
    }
    else
    {
        pr_warn("Unknown interrupt number: %d", frame->interrupt_number);
    }

    if (likely(current))
    {
        if (unlikely(current->status != THREAD_STATUS_RUNNING))
            pr_warn("Thread %d is not in 'running' state", current->tid);
    }

    frame->iret_params.eflags |= 0x200; // enable interrupts
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
        case EXCEPTION_DIVIDE_ERROR:
        case EXCEPTION_DEBUG:
        case EXCEPTION_NMI:
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

            uintptr_t fault_address;
            __asm__ volatile("mov %%cr2, %0" : "=r"(fault_address));

            bool present = (stack->error_code & 0x1) != 0;
            bool is_write = (stack->error_code & 0x2) != 0;
            bool is_user = (stack->error_code & 0x4) != 0;
            bool is_exec = false;

            if (fault_address < 1 KB)
            {
                x86_dump_registers(stack);
                mos_panic("%s NULL pointer dereference at " PTR_FMT " caused by instruction " PTR_FMT ".\n",
                          is_user ? "User" : "Kernel",       //
                          fault_address,                     //
                          (uintptr_t) stack->iret_params.eip //
                );
            }

            if (current_thread)
            {
                pr_emph("page fault: thread %d, process %s (pid %d) at " PTR_FMT ", instruction " PTR_FMT, //
                        current_thread->tid,                                                               //
                        current_process->name,                                                             //
                        current_process->pid,                                                              //
                        fault_address,                                                                     //
                        (uintptr_t) stack->iret_params.eip                                                 //
                );
                bool result = cow_handle_page_fault(fault_address, present, is_write, is_user, is_exec);

                if (result)
                {
                    pr_emph("page fault: resolved by CoW");
                    return;
                }
            }

            if (is_user && !is_write && present)
                pr_warn("'%s' privilege violation?", current_process->name);
            x86_dump_registers(stack);
            mos_panic("Page Fault: %s code at " PTR_FMT " is trying to %s a %s address " PTR_FMT, //
                      is_user ? "Userspace" : "Kernel",                                           //
                      (uintptr_t) stack->iret_params.eip,                                         //
                      is_write ? "write into" : "read from",                                      //
                      present ? "present" : "non-present",                                        //
                      fault_address);
            break;
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
    mos_panic("x86 %s:\nInterrupt #%d ('%s', error code %d)\n", intr_type, stack->interrupt_number, name, stack->error_code);
}

void irq_send_eoi(u8 irq)
{
    if (irq >= 8)
        port_outb(PIC2_COMMAND, PIC_EOI);
    port_outb(PIC1_COMMAND, PIC_EOI);
}

static void x86_handle_irq(x86_stack_frame *frame)
{
    int irq = frame->interrupt_number - IRQ_BASE;

    if (irq == 7 || irq == 15)
    {
        // these irqs may be fake ones, test it
        u8 pic = (irq < 8) ? PIC1 : PIC2;
        port_outb(pic + 3, 0x03);
        if ((port_inb(pic) & 0x80) != 0)
        {
            irq_send_eoi(irq);
            return;
        }
    }

    irq_send_eoi(irq);

    bool irq_handled = false;
    list_foreach(x86_irq_handler_t, handler, irq_handlers[irq])
    {
        irq_handled = true;
        handler->handler(irq);
    }

    if (unlikely(!irq_handled))
        pr_warn("IRQ %d not handled!", irq);
}
