// SPDX-License-Identifier: GPL-3.0-or-later

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
#include <mos/x86/descriptors/descriptor_types.h>
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

void platform_invalidate_tlb(void)
{
    x86_cpu_invlpg_all();
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

mm_context_t platform_mm_create_user_pgd(void)
{
    const size_t npages = ALIGN_UP_TO_PAGE(sizeof(x86_pg_infra_t)) / MOS_PAGE_SIZE;
    vmblock_t block = mm_alloc_pages(platform_info->kernel_mm, npages, MOS_ADDR_KERNEL_HEAP, VALLOC_DEFAULT, VM_RW);
    if (!block.vaddr)
    {
        mos_warn("failed to allocate page directory");
        return (mm_context_t){ 0 };
    }

    x86_pg_infra_t *infra = (x86_pg_infra_t *) block.vaddr;
    memzero(infra, sizeof(x86_pg_infra_t));
    mm_context_t handle = { 0 };
    // handle.pgd = (ptr_t) infra;
    MOS_UNREACHABLE();

    // // physical address of kernel page table
    // const ptr_t kpgtable_paddr = pg_get_mapped_paddr(x86_kpg_infra, (ptr_t) x86_kpg_infra->pgtable);

    // // this is a bit of a hack, but it's the easiest way that I can think of ...
    // const int kernel_pagedir_id_start = MOS_KERNEL_START_VADDR / MOS_PAGE_SIZE / 1024; // addr / (size of page) / (# pages of a page directory)
    // for (int i = kernel_pagedir_id_start; i < 1024; i++)
    // {
    //     x86_pde_t *pgd = &infra->pgdir[i];
    //     pgd->present = true;
    //     pgd->writable = true;
    //     pgd->usermode = false;
    //     // redirect it to the kernel page table
    //     // use pre-allocated (pre-calculated) physical address, otherwise some newly mapped pgdirs won't be applied correctly
    //     pgd->page_table_paddr = (kpgtable_paddr + i * 1024 * sizeof(x86_pte_t)) >> 12;
    // }
    // return handle;
}

void platform_mm_destroy_user_pgd(mm_context_t table)
{
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
    MOS_ASSERT_X(spinlock_is_locked(table->lock), "page table operations without lock");
    x86_mm_walk_page_table(table, vaddr, n, callback, arg);
}

u64 platform_arch_syscall(u64 syscall, u64 __maybe_unused arg1, u64 __maybe_unused arg2, u64 __maybe_unused arg3, u64 __maybe_unused arg4)
{
    switch (syscall)
    {
        case X86_SYSCALL_IOPL_ENABLE:
        {
            pr_info2("enabling IOPL for thread %ld", current_thread->tid);

            if (!current_process->platform_options)
                current_process->platform_options = kzalloc(sizeof(x86_process_options_t));

            x86_process_options_t *options = current_process->platform_options;
            options->iopl_enabled = true;
            return 0;
        }
        case X86_SYSCALL_IOPL_DISABLE:
        {
            pr_info2("disabling IOPL for thread %ld", current_thread->tid);

            if (!current_process->platform_options)
                current_process->platform_options = kzalloc(sizeof(x86_process_options_t));

            x86_process_options_t *options = current_process->platform_options;
            options->iopl_enabled = false;
            return 0;
        }
        case X86_SYSCALL_MAP_VGA_MEMORY:
        {
            pr_info2("mapping VGA memory for thread %ld", current_thread->tid);
            static ptr_t vga_paddr = X86_VIDEO_DEVICE_PADDR;

            if (once())
            {
                if (!pmm_find_reserved_region(vga_paddr))
                {
                    pr_info("reserving VGA address");
                    pmm_reserve_address(vga_paddr);
                }
            }

            mm_context_t *ctx = current_process->mm;

            const ptr_t vaddr = mm_get_free_pages(ctx, 1, MOS_ADDR_USER_MMAP, VALLOC_DEFAULT);
            const vmblock_t block = mm_replace_mapping(ctx, vaddr, vga_paddr / MOS_PAGE_SIZE, 1, VM_USER_RW);
            process_attach_mmap(current_process, block, VMTYPE_MMAP, (vmap_flags_t){ .fork_mode = VMAP_FORK_SHARED });
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
