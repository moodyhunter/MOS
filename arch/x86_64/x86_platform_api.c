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

u64 platform_arch_syscall(u64 syscall, u64 __maybe_unused arg1, u64 __maybe_unused arg2, u64 __maybe_unused arg3, u64 __maybe_unused arg4)
{
    switch (syscall)
    {
        case X86_SYSCALL_IOPL_ENABLE:
        {
            mos_debug(syscall, "enabling IOPL for thread %pt", (void *) current_thread);
            current_process->platform_options.iopl = true;
            return 0;
        }
        case X86_SYSCALL_IOPL_DISABLE:
        {
            mos_debug(syscall, "disabling IOPL for thread %pt", (void *) current_thread);
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

void platform_jump_to_signal_handler(const sigreturn_data_t *sigreturn_data, sigaction_t *sa)
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
