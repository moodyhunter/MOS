// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/paging/pml_types.hpp"
#include "mos/platform/platform_defs.hpp"
#include "mos/tasks/signal.hpp"
#include "mos/x86/devices/rtc.hpp"

#include <mos/lib/sync/spinlock.hpp>
#include <mos/mm/paging/paging.hpp>
#include <mos/mm/physical/pmm.hpp>
#include <mos/mos_global.h>
#include <mos/platform/platform.hpp>
#include <mos/platform_syscall.h>
#include <mos/syslog/printk.hpp>
#include <mos/tasks/process.hpp>
#include <mos/tasks/task_types.hpp>
#include <mos/x86/cpu/cpu.hpp>
#include <mos/x86/delays.hpp>
#include <mos/x86/devices/port.hpp>
#include <mos/x86/interrupt/apic.hpp>
#include <mos/x86/mm/paging_impl.hpp>
#include <mos/x86/tasks/context.hpp>
#include <mos/x86/x86_interrupt.hpp>
#include <mos/x86/x86_platform.hpp>
#include <mos_stdio.hpp>
#include <mos_stdlib.hpp>
#include <mos_string.hpp>

[[noreturn]] void platform_shutdown(void)
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
    return x86_cpuid(b, 1, 0) >> 24;
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
    snprintf(*per_cpu(datetime_str), sizeof(datetime_str_t), "%d-%02d-%02d %02d:%02d:%02d", time.year, time.month, time.day, time.hour, time.minute, time.second);
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

void platform_switch_mm(const MMContext *mm)
{
    x86_cpu_set_cr3(pgd_pfn(mm->pgd) * MOS_PAGE_SIZE);
}

platform_regs_t *platform_thread_regs(Thread *thread)
{
    return (platform_regs_t *) (thread->k_stack.top - sizeof(platform_regs_t));
}

void platform_dump_thread_kernel_stack(const Thread *thread)
{
    if (!thread)
    {
        pr_warn("thread is null, cannot dump its stack");
        return;
    }

    if (thread->state != THREAD_STATE_BLOCKED)
    {
        pr_emph("thread %pt is not blocked, cannot dump stack", thread);
        return;
    }

    ptr_t *rbp_ptr = (ptr_t *) thread->k_stack.head;
    rbp_ptr += 6; // 6 registers pushed by x86_context_switch_impl
    x86_dump_stack_at(*rbp_ptr, false);
}

[[noreturn]] void platform_return_to_userspace(platform_regs_t *regs)
{
    x86_interrupt_return_impl(regs);
}

u64 platform_arch_syscall(u64 syscall, u64 __maybe_unused arg1, u64 __maybe_unused arg2, u64 __maybe_unused arg3, u64 __maybe_unused arg4)
{
    switch (syscall)
    {
        case X86_SYSCALL_IOPL_ENABLE:
        {
            pr_dinfo2(syscall, "enabling IOPL for thread %pt", current_thread);
            current_process->platform_options.iopl = true;
            platform_thread_regs(current_thread)->eflags |= 0x3000;
            return 0;
        }
        case X86_SYSCALL_IOPL_DISABLE:
        {
            pr_dinfo2(syscall, "disabling IOPL for thread %pt", current_thread);
            current_process->platform_options.iopl = false;
            platform_thread_regs(current_thread)->eflags &= ~0x3000;
            return 0;
        }
        case X86_SYSCALL_SET_FS_BASE:
        {
            current_thread->platform_options.fs_base = arg1;
            x86_set_fsbase(current_thread);
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

void platform_jump_to_signal_handler(const platform_regs_t *regs, const sigreturn_data_t *sigreturn_data, const sigaction_t *sa)
{
    current_thread->u_stack.head = regs->sp - 128;

    // backup previous frame
    stack_push_val(&current_thread->u_stack, *regs);
    stack_push_val(&current_thread->u_stack, *sigreturn_data);

    // Set up the new context
    platform_regs_t ret_regs = *regs;
    ret_regs.ip = (ptr_t) sa->handler;
    stack_push_val(&current_thread->u_stack, (ptr_t) sa->sa_restorer); // the return address

    ret_regs.di = sigreturn_data->signal; // arg1
    ret_regs.sp = current_thread->u_stack.head;
    x86_interrupt_return_impl(&ret_regs);
}

void platform_restore_from_signal_handler(void *sp)
{
    current_thread->u_stack.head = (ptr_t) sp;
    sigreturn_data_t data;
    platform_regs_t regs;
    stack_pop_val(&current_thread->u_stack, data);
    stack_pop_val(&current_thread->u_stack, regs);

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

void platform_syscall_setup_restart_context(platform_regs_t *regs, reg_t syscall_nr)
{
    regs->ax = syscall_nr;
    regs->ip -= 2; // replay the 'syscall' or 'int 0x88' instruction
}

void platform_syscall_store_retval(platform_regs_t *regs, reg_t result)
{
    regs->ax = result;
}
