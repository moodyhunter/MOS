// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/platform/platform.h"

#include "mos/misc/panic.h"
#include "mos/syslog/printk.h"

#include <mos/mos_global.h>
#include <mos/types.h>

#define __weak__ [[gnu::weak]]

__weak__ noreturn void platform_ap_entry(u64 arg)
{
    MOS_UNUSED(arg);
    mos_panic("platform_ap_entry not implemented");
}

// Platform Machine APIs
__weak__ noreturn void platform_shutdown(void)
{
    mos_panic("platform_shutdown not implemented");
}

__weak__ void platform_dump_regs(platform_regs_t *regs)
{
    MOS_UNUSED(regs);
    pr_emerg("platform_dump_regs() not implemented");
}

__weak__ void platform_dump_stack(platform_regs_t *regs)
{
    MOS_UNUSED(regs);
    pr_emerg("platform_dump_stack() not implemented");
}

__weak__ void platform_dump_current_stack()
{
    pr_emerg("platform_dump_current_stack() not implemented");
}

__weak__ void platform_dump_thread_kernel_stack(const thread_t *thread)
{
    MOS_UNUSED(thread);
    pr_emerg("platform_dump_thread_kernel_stack() not implemented");
}

// Platform Timer/Clock APIs
__weak__ void platform_get_time(timeval_t *val)
{
    MOS_UNUSED(val);
    pr_emerg("platform_get_time() not implemented");
}

// Platform CPU APIs
__weak__ noreturn void platform_halt_cpu(void)
{
    pr_emerg("platform_halt_cpu() not implemented");
    while (1)
        ;
}

__weak__ void platform_invalidate_tlb(ptr_t vaddr)
{
    MOS_UNUSED(vaddr);
    pr_emerg("platform_invalidate_tlb() not implemented");
}

__weak__ u32 platform_current_cpu_id(void)
{
    return 0;
}

__weak__ void platform_cpu_idle(void)
{
    pr_emerg("platform_cpu_idle() not implemented");
}

__weak__ u64 platform_get_timestamp(void)
{
    pr_emerg("platform_get_timestamp() not implemented, returning 0");
    return 0;
}

__weak__ datetime_str_t *platform_get_datetime_str(void)
{
    pr_emerg("platform_get_datetime_str() not implemented");
    return nullptr;
}

// Platform Interrupt APIs, default implementation does nothing
__weak__ void platform_interrupt_enable(void)
{
    pr_emerg("platform_interrupt_enable() not implemented");
}

__weak__ void platform_interrupt_disable(void)
{
    pr_emerg("platform_interrupt_disable() not implemented");
}

// Platform-Specific syscall APIs, does nothing by default
__weak__ u64 platform_arch_syscall(u64 syscall, u64 arg1, u64 arg2, u64 arg3, u64 arg4)
{
    MOS_UNUSED(syscall);
    MOS_UNUSED(arg1);
    MOS_UNUSED(arg2);
    MOS_UNUSED(arg3);
    MOS_UNUSED(arg4);
    pr_emerg("platform_arch_syscall() not implemented");
    return 0;
}

// Platform-Specific IPI (Inter-Processor Interrupt) APIs
__weak__ void platform_ipi_send(u8 target_cpu, ipi_type_t type)
{
    MOS_UNUSED(target_cpu);
    MOS_UNUSED(type);
    pr_emerg("platform_ipi_send() not implemented");
}

// Signal Handler APIs, panic if not implemented

__weak__ noreturn void platform_jump_to_signal_handler(const platform_regs_t *regs, const sigreturn_data_t *sigreturn_data, const sigaction_t *sa)
{
    MOS_UNUSED(regs);
    MOS_UNUSED(sigreturn_data);
    MOS_UNUSED(sa);
    mos_panic("platform_jump_to_signal_handler() not implemented");
}

__weak__ noreturn void platform_restore_from_signal_handler(void *sp)
{
    MOS_UNUSED(sp);
    mos_panic("platform_restore_from_signal_handler() not implemented");
}

__weak__ void platform_syscall_setup_restart_context(platform_regs_t *regs, reg_t syscall_nr)
{
    MOS_UNUSED(regs);
    MOS_UNUSED(syscall_nr);
    mos_panic("platform_syscall_setup_restart_context() not implemented");
}

__weak__ void platform_syscall_store_retval(platform_regs_t *regs, reg_t result)
{
    MOS_UNUSED(regs);
    MOS_UNUSED(result);
    mos_panic("platform_syscall_store_retval() not implemented");
}
