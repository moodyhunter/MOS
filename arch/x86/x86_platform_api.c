// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/string.h"
#include "mos/mm/kmalloc.h"
#include "mos/mm/paging/paging.h"
#include "mos/mos_global.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/x86/acpi/acpi.h"
#include "mos/x86/cpu/cpu.h"
#include "mos/x86/delays.h"
#include "mos/x86/devices/port.h"
#include "mos/x86/mm/paging.h"
#include "mos/x86/mm/paging_impl.h"
#include "mos/x86/tasks/context.h"
#include "mos/x86/x86_interrupt.h"

noreturn void platform_shutdown(void)
{
    x86_disable_interrupts();
    port_outw(0x604, 0x2000);
    x86_cpu_halt();
    while (1)
        ;
}

void platform_halt_cpu(void)
{
    x86_cpu_halt();
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

void platform_interrupt_enable(void)
{
    x86_enable_interrupts();
}

void platform_interrupt_disable(void)
{
    x86_disable_interrupts();
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

paging_handle_t platform_mm_create_user_pgd(void)
{
    const size_t npages = ALIGN_UP_TO_PAGE(sizeof(x86_pg_infra_t)) / MOS_PAGE_SIZE;
    vmblock_t block = mm_alloc_pages(current_cpu->pagetable, npages, PGALLOC_HINT_KHEAP, VM_RW);
    x86_pg_infra_t *infra = (x86_pg_infra_t *) block.vaddr;
    memzero(infra, sizeof(x86_pg_infra_t));
    paging_handle_t handle;
    handle.pgd = (uintptr_t) infra;

    // physical address of kernel page table
    const uintptr_t kpgtable_paddr = pg_page_get_mapped_paddr(x86_kpg_infra, (uintptr_t) x86_kpg_infra->pgtable);

    // this is a bit of a hack, but it's the easiest way that I can think of ...
    const int kernel_pagedir_id_start = MOS_KERNEL_START_VADDR / MOS_PAGE_SIZE / 1024; // addr / (size of page) / (# pages of a page directory)
    for (int i = kernel_pagedir_id_start; i < 1024; i++)
    {
        x86_pgdir_entry *pgd = &infra->pgdir[i];
        pgd->present = true;
        pgd->writable = true;
        pgd->usermode = false;
        // redirect it to the kernel page table
        // use pre-allocated (pre-calculated) physical address, otherwise some newly mapped pgdirs won't be applied correctly
        pgd->page_table_paddr = (kpgtable_paddr + i * 1024 * sizeof(x86_pgtable_entry)) >> 12;
    }
    return handle;
}

void platform_mm_destroy_user_pgd(paging_handle_t table)
{
    kfree((void *) table.pgd);
}

void platform_context_setup(thread_t *thread, thread_entry_t entry, void *arg)
{
    x86_setup_thread_context(thread, entry, arg);
}

void platform_context_copy(platform_context_t *from, platform_context_t **to)
{
    x86_copy_thread_context(from, to);
}

void platform_switch_to_scheduler(uintptr_t *old_stack, uintptr_t new_stack)
{
    x86_switch_to_scheduler(old_stack, new_stack);
}

void platform_switch_to_thread(uintptr_t *old_stack, thread_t *new_thread)
{
    x86_switch_to_thread(old_stack, new_thread);
}

void platform_mm_map_pages(paging_handle_t table, vmblock_t block)
{
    mos_debug("paging: mapping %zu pages (" PTR_FMT "->" PTR_FMT ") @ table " PTR_FMT, block.npages, block.vaddr, block.paddr, table.pgd);

    x86_pg_infra_t *infra = x86_get_pg_infra(table);
    for (size_t i = 0; i < block.npages; i++)
        pg_do_map_page(infra, block.vaddr + i * MOS_PAGE_SIZE, block.paddr + i * MOS_PAGE_SIZE, block.flags);
}

void platform_mm_unmap_pages(paging_handle_t table, uintptr_t vaddr_start, size_t n_pages)
{
    mos_debug("paging: unmapping %zu pages starting at " PTR_FMT " @ table " PTR_FMT, n_pages, vaddr_start, table.pgd);

    x86_pg_infra_t *infra = x86_get_pg_infra(table);
    for (size_t i = 0; i < n_pages; i++)
        pg_do_unmap_page(infra, vaddr_start + i * MOS_PAGE_SIZE);
}

vmblock_t platform_mm_get_block_info(paging_handle_t table, uintptr_t vaddr, size_t npages)
{
    x86_pg_infra_t *infra = x86_get_pg_infra(table);
    vmblock_t block;
    block.vaddr = vaddr;
    block.paddr = pg_page_get_mapped_paddr(infra, vaddr);
    block.npages = npages;
    block.flags = pg_page_get_flags(infra, vaddr);
    return block;
}

vmblock_t platform_mm_copy_maps(paging_handle_t from, uintptr_t fvaddr, paging_handle_t to, uintptr_t tvaddr, size_t npages)
{
    x86_pg_infra_t *from_infra = x86_get_pg_infra(from);
    x86_pg_infra_t *to_infra = x86_get_pg_infra(to);

    for (size_t i = 0; i < npages; i++)
    {
        uintptr_t from_vaddr = fvaddr + i * MOS_PAGE_SIZE;
        uintptr_t to_vaddr = tvaddr + i * MOS_PAGE_SIZE;
        uintptr_t paddr = pg_page_get_mapped_paddr(from_infra, from_vaddr);
        vm_flags flags = pg_page_get_flags(from_infra, from_vaddr);
        pg_do_map_page(to_infra, to_vaddr, paddr, flags);
    }

    vmblock_t block = {
        .vaddr = tvaddr,
        .npages = npages,
        .flags = pg_page_get_flags(from_infra, fvaddr),
    };

    return block;
}

void platform_mm_flag_pages(paging_handle_t table, uintptr_t vaddr, size_t n, vm_flags flags)
{
    x86_pg_infra_t *infra = x86_get_pg_infra(table);
    pg_page_flag(infra, vaddr, n, flags);
}

vm_flags platform_mm_get_flags(paging_handle_t table, uintptr_t vaddr)
{
    x86_pg_infra_t *infra = x86_get_pg_infra(table);
    return pg_page_get_flags(infra, vaddr);
}
