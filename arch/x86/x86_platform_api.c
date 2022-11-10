// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/string.h"
#include "mos/mm/kmalloc.h"
#include "mos/mos_global.h"
#include "mos/platform/platform.h"
#include "mos/x86/acpi/acpi.h"
#include "mos/x86/cpu/cpu.h"
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
    vmblock_t block = pg_page_alloc(x86_kpg_infra, ALIGN_UP_TO_PAGE(sizeof(x86_pg_infra_t)) / MOS_PAGE_SIZE, PGALLOC_HINT_KHEAP, VM_READ | VM_WRITE);
    x86_pg_infra_t *infra = (x86_pg_infra_t *) block.vaddr;
    memset(infra, 0, sizeof(x86_pg_infra_t));
    paging_handle_t handle;
    handle.ptr = (uintptr_t) infra;

    // pg_do_map_pages(infra, 0, 0, 1, VM_NONE); // ! the zero page is not writable, nor readable by user
    pg_do_map_pages(infra, MOS_PAGE_SIZE, MOS_PAGE_SIZE, 1 MB / MOS_PAGE_SIZE - 1, VM_GLOBAL | VM_WRITE);

    // physical address of kernel page table
    const uintptr_t kpgtable_paddr = pg_page_get_mapped_paddr(x86_kpg_infra, (uintptr_t) x86_kpg_infra->pgtable);

    // this is a bit of a hack, but it's the easiest way that I can think of ...
    const int kernel_page_start = MOS_KERNEL_START_VADDR / (1024 * MOS_PAGE_SIZE);
    for (int i = kernel_page_start; i < 1024; i++)
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
    kfree((void *) table.ptr);
}

vmblock_t platform_mm_alloc_pages(paging_handle_t table, size_t npages, pgalloc_hints hints, vm_flags vm_flags)
{
    x86_pg_infra_t *kpg_infra = x86_get_pg_infra(table);
    return pg_page_alloc(kpg_infra, npages, hints, vm_flags);
}

vmblock_t platform_mm_alloc_pages_at(paging_handle_t table, uintptr_t vaddr, size_t npages, vm_flags vflags)
{
    x86_pg_infra_t *kpg_infra = x86_get_pg_infra(table);
    return pg_page_alloc_at(kpg_infra, vaddr, npages, vflags);
}

vmblock_t platform_mm_get_free_pages(paging_handle_t table, size_t npages, pgalloc_hints hints)
{
    x86_pg_infra_t *kpg_infra = x86_get_pg_infra(table);
    return pg_page_get_free(kpg_infra, npages, hints);
}

vmblock_t platform_mm_copy_maps(paging_handle_t from, uintptr_t fvaddr, paging_handle_t to, uintptr_t tvaddr, size_t npages)
{
    x86_pg_infra_t *from_infra = x86_get_pg_infra(from);
    x86_pg_infra_t *to_infra = x86_get_pg_infra(to);

    // uintptr_t start_paddr = pg_page_get_mapped_paddr(from_infra, fvaddr);

    for (size_t i = 0; i < npages; i++)
    {
        uintptr_t from_vaddr = fvaddr + i * MOS_PAGE_SIZE;
        uintptr_t to_vaddr = tvaddr + i * MOS_PAGE_SIZE;
        // uintptr_t expected_paddr = start_paddr + i * MOS_PAGE_SIZE;
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

void platform_mm_unmap_pages(paging_handle_t table, uintptr_t vaddr, size_t n)
{
    x86_pg_infra_t *kpg_infra = x86_get_pg_infra(table);
    pg_do_unmap_pages(kpg_infra, vaddr, n);
}

void platform_mm_free_pages(paging_handle_t table, uintptr_t vaddr, size_t n)
{
    x86_pg_infra_t *kpg_infra = x86_get_pg_infra(table);
    pg_page_free(kpg_infra, vaddr, n);
}

void platform_mm_flag_pages(paging_handle_t table, uintptr_t vaddr, size_t n, vm_flags flags)
{
    x86_pg_infra_t *kpg_infra = x86_get_pg_infra(table);
    pg_page_flag(kpg_infra, vaddr, n, flags);
}

vm_flags platform_mm_get_flags(paging_handle_t table, uintptr_t vaddr)
{
    x86_pg_infra_t *kpg_infra = x86_get_pg_infra(table);
    return pg_page_get_flags(kpg_infra, vaddr);
}

void platform_context_setup(thread_t *thread, downwards_stack_t *proxy_stack, thread_entry_t entry, void *arg)
{
    x86_setup_thread_context(thread, proxy_stack, entry, arg);
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
