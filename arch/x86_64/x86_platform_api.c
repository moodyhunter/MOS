// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/memops.h"
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

pgd_t platform_mm_create_user_pgd(void)
{
    pmltop_t top = pml_create_table(pmltop);

    // map the upper half of the address space to the kernel
    for (int i = pml4_index(MOS_KERNEL_START_VADDR); i < MOS_PLATFORM_PML4_NPML3; i++)
    {
        const pml4e_t *kpml4e = &platform_info->kernel_mm->pgd.max.pml4.table[i];
        pml4e_t *pml4e = &top.table[i];
        pml4e->content = kpml4e->content;
    }

    return pgd_from_pmltop(top);
}

void platform_mm_destroy_user_pgd(pgd_t max)
{
    MOS_UNUSED(max);
    MOS_UNREACHABLE();
    // if (!table.pgd)
    // {
    //     mos_warn("invalid pgd");
    //     return;
    // }
    // kfree((void *) table.pgd);
}

void platform_context_setup(thread_t *thread, thread_entry_t entry, void *arg)
{
    x86_setup_thread_context(thread, entry, arg);
}

void platform_setup_forked_context(const thread_context_t *from, thread_context_t **to)
{
    x86_setup_forked_context(from, to);
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
        case X86_SYSCALL_MAP_VGA_MEMORY:
        {
            pr_info2("mapping VGA memory for thread %d", current_thread->tid);
            static ptr_t vga_paddr = X86_VIDEO_DEVICE_PADDR;

            if (once())
            {
                if (!pmm_find_reserved_region(vga_paddr))
                {
                    pr_info("reserving VGA address");
                    pmm_reserve_address(vga_paddr);
                }
            }

            mm_context_t *mmctx = current_process->mm;

            spinlock_acquire(&mmctx->mm_lock);
            const ptr_t vaddr = mm_get_free_vaddr(mmctx, 1, MOS_ADDR_USER_MMAP, VALLOC_DEFAULT);
            const vmblock_t block = mm_map_pages_locked(mmctx, vaddr, vga_paddr / MOS_PAGE_SIZE, 1, VM_USER_RW);
            mm_attach_vmap(current_process->mm, mm_new_vmap(block, VMTYPE_MMAP, (vmap_flags_t){ .fork_mode = VMAP_FORK_SHARED }));
            spinlock_release(&mmctx->mm_lock);
            return block.vaddr;
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