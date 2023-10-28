// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/mm.h"
#include "mos/mm/paging/pml_types.h"
#include "mos/mm/paging/pmlx/pml4.h"
#include "mos/platform/platform_defs.h"
#include "mos/tasks/signal.h"
#include "mos/x86/devices/rtc.h"

#include <mos/lib/sync/spinlock.h>
#include <mos/mm/paging/paging.h>
#include <mos/mm/physical/pmm.h>
#include <mos/mos_global.h>
#include <mos/platform/platform.h>
#include <mos/platform_syscall.h>
#include <mos/printk.h>
#include <mos/tasks/process.h>
#include <mos/tasks/task_types.h>
#include <mos/x86/cpu/cpu.h>
#include <mos/x86/delays.h>
#include <mos/x86/devices/port.h>
#include <mos/x86/interrupt/apic.h>
#include <mos/x86/mm/paging_impl.h>
#include <mos/x86/tasks/context.h>
#include <mos/x86/x86_interrupt.h>
#include <mos/x86/x86_platform.h>
#include <mos_stdio.h>
#include <mos_stdlib.h>
#include <mos_string.h>

noreturn void platform_shutdown(void)
{
    platform_interrupt_disable();
    port_outw(0x604, 0x2000);
    x86_cpu_halt();
    while (1)
        ;
}

void platform_halt_cpu(void)
{
    x86_cpu_halt();
}

void platform_invalidate_tlb(ptr_t vaddr)
{
    if (!vaddr)
        x86_cpu_invlpg_all();
    else
        x86_cpu_invlpg(vaddr);
}

u32 platform_current_cpu_id(void)
{
    return x86_cpuid(b, leaf = 1) >> 24;
}

void platform_msleep(u64 ms)
{
    mdelay(ms);
}

void platform_usleep(u64 us)
{
    udelay(us);
}

void platform_cpu_idle(void)
{
    __asm__ volatile("hlt");
}

u64 platform_get_timestamp()
{
    return rdtsc();
}

datetime_str_t *platform_get_datetime_str(void)
{
    static PER_CPU_DECLARE(datetime_str_t, datetime_str);

    timeval_t time;
    platform_get_time(&time);
    snprintf((char *) per_cpu(datetime_str), sizeof(datetime_str_t), "%d-%02d-%02d %02d:%02d:%02d", time.year, time.month, time.day, time.hour, time.minute, time.second);
    return per_cpu(datetime_str);
}

void platform_interrupt_enable(void)
{
    __asm__ volatile("sti");
}

void platform_interrupt_disable(void)
{
    __asm__ volatile("cli");
}

bool platform_irq_handler_install(u32 irq, irq_handler handler)
{
    return x86_install_interrupt_handler(irq, handler);
}

void platform_irq_handler_remove(u32 irq, irq_handler handler)
{
    // TODO: implement
    MOS_UNUSED(irq);
    MOS_UNUSED(handler);
}

void platform_switch_mm(mm_context_t *mm)
{
    x86_cpu_set_cr3(pgd_pfn(mm->pgd) * MOS_PAGE_SIZE);
}

platform_regs_t *platform_thread_regs(const thread_t *thread)
{
    return (platform_regs_t *) (thread->k_stack.top - sizeof(platform_regs_t));
}

noreturn void platform_return_to_userspace(platform_regs_t *regs)
{
    x86_interrupt_return_impl(regs);
}

u64 platform_arch_syscall(u64 syscall, u64 __maybe_unused arg1, u64 __maybe_unused arg2, u64 __maybe_unused arg3, u64 __maybe_unused arg4)
{
    switch (syscall)
    {
        case X86_SYSCALL_IOPL_ENABLE:
        {
            pr_dinfo2(syscall, "enabling IOPL for thread %pt", (void *) current_thread);
            current_process->platform_options.iopl = true;
            return 0;
        }
        case X86_SYSCALL_IOPL_DISABLE:
        {
            pr_dinfo2(syscall, "disabling IOPL for thread %pt", (void *) current_thread);
            current_process->platform_options.iopl = false;
            return 0;
        }
        case X86_SYSCALL_SET_FS_BASE:
        {
            current_thread->platform_options.fs_base = arg1;
            x86_update_current_fsbase();
            return 0;
        }
        case X86_SYSCALL_SET_GS_BASE:
        {
            current_thread->platform_options.gs_base = arg1;
            // x86_update_current_gsbase();
            MOS_UNIMPLEMENTED("set_gs_base");
            return 0;
        }
        default:
        {
            pr_warn("unknown arch-specific syscall %llu", syscall);
            return -1;
        }
    }
}

void platform_ipi_send(u8 target, ipi_type_t type)
{
    if (target == TARGET_CPU_ALL)
        lapic_interrupt(IPI_BASE + type, 0xff, APIC_DELIVER_MODE_NORMAL, LAPIC_DEST_MODE_PHYSICAL, LAPIC_SHORTHAND_ALL_EXCLUDING_SELF);
    else
        lapic_interrupt(IPI_BASE + type, target, APIC_DELIVER_MODE_NORMAL, LAPIC_DEST_MODE_PHYSICAL, LAPIC_SHORTHAND_NONE);
}

void platform_jump_to_signal_handler(const sigreturn_data_t *sigreturn_data, const sigaction_t *sa)
{
    platform_regs_t *regs = platform_thread_regs(current_thread);

    // avoid x86_64 ABI red zone
    current_thread->u_stack.head = regs->sp - 128;

    // backup previous frame
    stack_push(&current_thread->u_stack, regs, sizeof(platform_regs_t));
    stack_push_val(&current_thread->u_stack, *sigreturn_data);

    // Set up the new context
    regs->ip = (ptr_t) sa->handler;
    stack_push_val(&current_thread->u_stack, sa->sigreturn_trampoline); // the return address

    regs->di = sigreturn_data->signal; // arg1
    regs->sp = current_thread->u_stack.head;
    x86_interrupt_return_impl(regs);
}

void platform_restore_from_signal_handler(void *sp)
{
    current_thread->u_stack.head = (ptr_t) sp;
    sigreturn_data_t data;
    platform_regs_t regs;
    stack_pop_val(&current_thread->u_stack, data);
    stack_pop(&current_thread->u_stack, sizeof(platform_regs_t), &regs);

    signal_on_returned(&data);
    x86_interrupt_return_impl(&regs);
}

void platform_get_time(timeval_t *time)
{
    rtc_read_time(time);
}

void platform_dump_regs(platform_regs_t *frame)
{
    pr_emph("General Purpose Registers:\n"
            "  RAX: " PTR_FMT " RBX: " PTR_FMT " RCX: " PTR_FMT " RDX: " PTR_FMT "\n"
            "  RSI: " PTR_FMT " RDI: " PTR_FMT " RBP: " PTR_FMT " RSP: " PTR_FMT "\n"
            "  R8:  " PTR_FMT " R9:  " PTR_FMT " R10: " PTR_FMT " R11: " PTR_FMT "\n"
            "  R12: " PTR_FMT " R13: " PTR_FMT " R14: " PTR_FMT " R15: " PTR_FMT "\n"
            "  IP:  " PTR_FMT "\n"
            "Context:\n"
            "  EFLAGS:       " PTR_FMT "\n"
            "  Instruction:  0x%lx:" PTR_FMT "\n"
            "  Stack:        0x%lx:" PTR_FMT,
            frame->ax, frame->bx, frame->cx, frame->dx,     //
            frame->si, frame->di, frame->bp, frame->sp,     //
            frame->r8, frame->r9, frame->r10, frame->r11,   //
            frame->r12, frame->r13, frame->r14, frame->r15, //
            frame->ip,                                      //
            frame->eflags,                                  //
            frame->cs, frame->ip,                           //
            frame->ss, frame->sp                            //
    );
}
