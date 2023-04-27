// SPDX-License-Identifier: GPL-3.0-or-later
#include <mos/interrupt/ipi.h>
#include <mos/mm/cow.h>
#include <mos/mm/kmalloc.h>
#include <mos/platform/platform.h>
#include <mos/printk.h>
#include <mos/syscall/dispatcher.h>
#include <mos/x86/cpu/cpu.h>
#include <mos/x86/devices/port.h>
#include <mos/x86/interrupt/apic.h>
#include <mos/x86/tasks/context.h>
#include <mos/x86/x86_interrupt.h>
#include <mos/x86/x86_platform.h>

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

void x86_disable_interrupts(void)
{
    __asm__ volatile("cli");
}

void x86_enable_interrupts(void)
{
    __asm__ volatile("sti");
}

bool x86_install_interrupt_handler(u32 irq, void (*handler)(u32 irq))
{
    MOS_ASSERT(irq < IRQ_MAX_COUNT);
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
            frame->eax, frame->ebx, frame->ecx, frame->edx,             //
            frame->esi, frame->edi, frame->ebp, frame->iret_params.esp, //
            frame->iret_params.eip,                                     //
            frame->ds, frame->es, frame->fs, frame->gs,                 //
            frame->iret_params.eflags,                                  //
            frame->iret_params.cs, frame->iret_params.eip,              //
            frame->iret_params.ss, frame->iret_params.esp               //
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
            bool is_exec = false;

            thread_t *current = current_thread;

            if (fault_address < 1 KB)
            {
                x86_dump_registers(stack);
                mos_panic("thread %ld (%s), process %ld (%s), %s NULL pointer dereference at " PTR_FMT " caused by instruction " PTR_FMT,
                          current ? current->tid : 0,                //
                          current ? current->name : "<none>",        //
                          current ? current->owner->pid : 0,         //
                          current ? current->owner->name : "<none>", //
                          is_user ? "User" : "Kernel",               //
                          fault_address,                             //
                          (ptr_t) stack->iret_params.eip             //
                );
            }

            if (current)
            {
                if (MOS_DEBUG_FEATURE(cow))
                {
                    pr_emph("%s page fault: thread %ld (%s), process %ld (%s) at " PTR_FMT ", instruction " PTR_FMT, //
                            is_user ? "user" : "kernel",                                                             //
                            current->tid,                                                                            //
                            current->name,                                                                           //
                            current->owner->pid,                                                                     //
                            current->owner->name,                                                                    //
                            fault_address,                                                                           //
                            (ptr_t) stack->iret_params.eip                                                           //
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

            if (is_user && !is_write && present)
                pr_warn("'%s' trying to read kernel memory?", current_process->name);
            x86_dump_registers(stack);
            mos_panic("Page Fault: %s code at " PTR_FMT " is trying to %s a %s address " PTR_FMT, //
                      is_user ? "Userspace" : "Kernel",                                           //
                      (ptr_t) stack->iret_params.eip,                                             //
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
    mos_panic("x86 %s:\nInterrupt #%d ('%s', error code %d)", intr_type, stack->interrupt_number, name, stack->error_code);
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

void x86_handle_interrupt(u32 esp)
{
    x86_stack_frame *frame = (x86_stack_frame *) esp;

    thread_t *current = current_thread;
    if (likely(current))
    {
        x86_thread_context_t *context = container_of(current->context, x86_thread_context_t, inner);
        context->regs = *frame;
        context->inner.instruction = frame->iret_params.eip;
        context->inner.stack = frame->iret_params.esp;
    }

    if (frame->interrupt_number < IRQ_BASE)
        x86_handle_exception(frame);
    else if (frame->interrupt_number >= IRQ_BASE && frame->interrupt_number < IRQ_BASE + IRQ_MAX)
        x86_handle_irq(frame);
    else if (frame->interrupt_number >= IPI_BASE && frame->interrupt_number < IPI_BASE + IPI_TYPE_MAX)
        ipi_do_handle((ipi_type_t) (frame->interrupt_number - IPI_BASE));
    else if (frame->interrupt_number == MOS_SYSCALL_INTR)
        frame->eax = dispatch_syscall(frame->eax, frame->ebx, frame->ecx, frame->edx, frame->esi, frame->edi, frame->ebp);
    else
        pr_warn("Unknown interrupt number: %d", frame->interrupt_number);

    if (likely(current))
    {
        MOS_ASSERT_X(current->state == THREAD_STATE_RUNNING, "thread %ld is not in 'running' state", current->tid);

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

    frame->iret_params.eflags |= 0x200; // enable interrupts
}
