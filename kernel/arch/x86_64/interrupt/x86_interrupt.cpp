// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/interrupt/interrupt.hpp"
#include "mos/ksyscall_entry.hpp"
#include "mos/misc/panic.hpp"
#include "mos/misc/profiling.hpp"
#include "mos/tasks/signal.hpp"

#include <mos/interrupt/ipi.hpp>
#include <mos/lib/structures/list.hpp>
#include <mos/mm/cow.hpp>
#include <mos/platform/platform.hpp>
#include <mos/syscall/dispatcher.h>
#include <mos/syslog/printk.hpp>
#include <mos/x86/cpu/cpu.hpp>
#include <mos/x86/devices/port.hpp>
#include <mos/x86/interrupt/apic.hpp>
#include <mos/x86/tasks/context.hpp>
#include <mos/x86/x86_interrupt.hpp>
#include <mos/x86/x86_platform.hpp>
#include <mos_stdio.hpp>
#include <mos_stdlib.hpp>

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

static void x86_handle_nmi(const platform_regs_t *regs)
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

    platform_dump_regs(regs);
    mos_panic("NMI received");
}

static void x86_handle_exception(const platform_regs_t *regs)
{
    MOS_ASSERT(regs->interrupt_number < EXCEPTION_COUNT);

    const char *name = x86_exception_names[regs->interrupt_number];
    const char *intr_type = "";

    // Faults: These can be corrected and the program may continue as if nothing happened.
    // Traps:  Traps are reported immediately after the execution of the trapping instruction.
    // Aborts: Some severe unrecoverable error.
    switch ((x86_exception_enum_t) regs->interrupt_number)
    {
        case EXCEPTION_NMI:
        {
            x86_handle_nmi(regs);
            break;
        }
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
                     lapic_get_id(), name, regs->interrupt_number, regs->ip, drx[0], drx[1], drx[2], drx[3], drx[4], drx[5]);

            return;
        }
        case EXCEPTION_INVALID_OPCODE:
        {
            intr_type = "fault";

            if (MOS_IN_RANGE(regs->ip, (ptr_t) __MOS_KERNEL_CODE_START, (ptr_t) __MOS_KERNEL_CODE_END))
            {
                // kernel mode invalid opcode, search for a bug entry
                try_handle_kernel_panics(regs->ip);
                mos_panic("Invalid opcode in kernel mode");
            }
            break;
        }
        case EXCEPTION_DIVIDE_ERROR:
        case EXCEPTION_OVERFLOW:
        case EXCEPTION_BOUND_RANGE_EXCEEDED:
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

            pagefault_t info = {
                .is_present = (regs->error_code & 0x1) != 0,
                .is_write = (regs->error_code & 0x2) != 0,
                .is_user = (regs->error_code & 0x4) != 0,
                .is_exec = (regs->error_code & 0x10) != 0,
                .ip = regs->ip,
                .regs = regs,
                .backing_page = nullptr,
            };

            mm_handle_fault(x86_cpu_get_cr2(), &info);
            goto done;
        }

        case EXCEPTION_DOUBLE_FAULT:
        case EXCEPTION_MACHINE_CHECK:
        {
            intr_type = "abort";
            break;
        }
        case EXCEPTION_MAX:
        case EXCEPTION_COUNT:
        {
            MOS_UNREACHABLE();
        }
    }

    if (current_thread)
    {
        pr_emerg("cpu %d: %s (%lu) at " PTR_FMT " (error code %lu)", lapic_get_id(), name, regs->interrupt_number, regs->ip, regs->error_code);
        signal_send_to_thread(current_thread, SIGKILL);
        platform_dump_regs(regs);
        platform_dump_current_stack();
        platform_dump_stack(regs);
    }
    else
    {
        platform_dump_regs(regs);
        mos_panic("x86 %s:\nInterrupt #%lu ('%s', error code %lu)", intr_type, regs->interrupt_number, name, regs->error_code);
    }

done:
    return;
}

static void x86_handle_irq(platform_regs_t *frame)
{
    lapic_eoi();
    const int irq = frame->interrupt_number - IRQ_BASE;
    interrupt_entry(irq);
}

extern "C" platform_regs_t *x86_interrupt_entry(ptr_t rsp)
{
    platform_regs_t *frame = (platform_regs_t *) rsp;
    current_cpu->interrupt_regs = frame;

    reg_t syscall_ret = 0, syscall_nr = 0;

    const pf_point_t ev = profile_enter();

    if (frame->interrupt_number < IRQ_BASE)
        x86_handle_exception(frame);
    else if (frame->interrupt_number >= IRQ_BASE && frame->interrupt_number < IRQ_BASE + IRQ_MAX)
        x86_handle_irq(frame);
    else if (frame->interrupt_number >= IPI_BASE && frame->interrupt_number < IPI_BASE + IPI_TYPE_MAX)
        ipi_do_handle((ipi_type_t) (frame->interrupt_number - IPI_BASE)), lapic_eoi();
    else if (frame->interrupt_number == MOS_SYSCALL_INTR)
        syscall_nr = frame->ax, syscall_ret = ksyscall_enter(frame->ax, frame->bx, frame->cx, frame->dx, frame->si, frame->di, frame->r9);
    else
        pr_warn("Unknown interrupt number: %lu", frame->interrupt_number);

    profile_leave(ev, "x86.int.%lu", frame->interrupt_number);

    if (unlikely(!current_thread))
        return frame;

    // jump to signal handler if there is a pending signal, and if we are coming from userspace
    if (frame->cs & 0x3)
    {
        ptr<platform_regs_t> syscall_ret_regs;
        if (frame->interrupt_number == MOS_SYSCALL_INTR)
            syscall_ret_regs = signal_exit_to_user_prepare(frame, syscall_nr, syscall_ret);
        else
            syscall_ret_regs = signal_exit_to_user_prepare(frame);

        if (syscall_ret_regs)
            *frame = *syscall_ret_regs;
    }

    // x86_interrupt_return_impl(frame);
    // MOS_UNREACHABLE();
    return frame;
}
