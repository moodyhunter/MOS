// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/mm.h"
#include "mos/mm/paging/pml_types.h"
#include "mos/mm/paging/pmlx/pml4.h"
#include "mos/mm/paging/table_ops.h"
#include "mos/platform/platform_defs.h"

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
#include <mos/x86/mm/paging.h>
#include <mos/x86/mm/paging_impl.h>
#include <mos/x86/tasks/context.h>
#include <mos/x86/x86_interrupt.h>
#include <mos/x86/x86_platform.h>
#include <stdlib.h>
#include <string.h>

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
    return x86_cpu_get_id();
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

void platform_context_setup(thread_t *thread, thread_entry_t entry, void *arg)
{
    x86_setup_thread_context(thread, entry, arg);
}

void platform_setup_forked_context(const thread_context_t *from, thread_context_t **to)
{
    x86_setup_forked_context(from, to);
}

void platform_mm_switch_pgd(pgd_t pgd)
{
    x86_cpu_set_cr3(pgd_pfn(pgd) * MOS_PAGE_SIZE);
}

void platform_switch_to_scheduler(ptr_t *old_stack, ptr_t new_stack)
{
    x86_switch_to_scheduler(old_stack, new_stack);
}

void platform_switch_to_thread(ptr_t *old_stack, const thread_t *new_thread, switch_flags_t switch_flags)
{
    x86_switch_to_thread(old_stack, new_thread, switch_flags);
}

void platform_mm_iterate_table(mm_context_t *table, ptr_t vaddr, size_t n, pgt_iteration_callback_t callback, void *arg)
{
    MOS_ASSERT_X(spinlock_is_locked(&table->mm_lock), "page table operations without lock");
    x86_mm_walk_page_table(table, vaddr, n, callback, arg);
}

u64 platform_arch_syscall(u64 syscall, u64 __maybe_unused arg1, u64 __maybe_unused arg2, u64 __maybe_unused arg3, u64 __maybe_unused arg4)
{
    switch (syscall)
    {
        case X86_SYSCALL_IOPL_ENABLE:
        {
            pr_info2("enabling IOPL for thread %d", current_thread->tid);

            if (!current_process->platform_options)
                current_process->platform_options = kzalloc(sizeof(x86_process_options_t));

            x86_process_options_t *options = current_process->platform_options;
            options->iopl_enabled = true;
            return 0;
        }
        case X86_SYSCALL_IOPL_DISABLE:
        {
            pr_info2("disabling IOPL for thread %d", current_thread->tid);

            if (!current_process->platform_options)
                current_process->platform_options = kzalloc(sizeof(x86_process_options_t));

            x86_process_options_t *options = current_process->platform_options;
            options->iopl_enabled = false;
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
