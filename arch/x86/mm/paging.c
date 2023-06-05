// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/mm/paging/dump.h>
#include <mos/mm/paging/paging.h>
#include <mos/platform/platform.h>
#include <mos/printk.h>
#include <mos/x86/mm/paging.h>
#include <mos/x86/mm/paging_impl.h>
#include <mos/x86/x86_platform.h>
#include <string.h>

static x86_pg_infra_t x86_kpg_infra_storage __aligned(MOS_PAGE_SIZE) = { 0 };
static spinlock_t x86_kernel_pgd_lock = SPINLOCK_INIT;

x86_pg_infra_t *const x86_kpg_infra = &x86_kpg_infra_storage;

void x86_mm_paging_init(void)
{
    // initialize the page directory
    memzero(x86_kpg_infra, sizeof(x86_pg_infra_t));
    x86_platform.kernel_pgd.pgd = (ptr_t) x86_kpg_infra;
    x86_platform.kernel_pgd.um_page_map = NULL; // a kernel page table does not have a user-mode page map
    x86_platform.kernel_pgd.pgd_lock = &x86_kernel_pgd_lock;
    current_cpu->pagetable = x86_platform.kernel_pgd;
}

void x86_mm_walk_page_table(paging_handle_t handle, ptr_t vaddr_start, size_t n_pages, pgt_iteration_callback_t callback, void *arg)
{
    ptr_t vaddr = vaddr_start;
    size_t n_pages_left = n_pages;

    const x86_pg_infra_t *pg = x86_get_pg_infra(handle);
    const pgt_iteration_info_t info = { .address_space = handle, .vaddr_start = vaddr_start, .npages = n_pages };

    pfn_t previous_pfn = 0;
    vm_flags previous_flags = 0;
    bool previous_present = false;
    vmblock_t previous_block = { .vaddr = vaddr_start, .npages = 0, .flags = 0, .address_space = handle };

    do
    {
        const id_t pgd_i = vaddr >> 22;
        const id_t pgt_i = (vaddr >> 12) & 0x3FF;

        const x86_pgdir_entry *pgd = (vaddr >= MOS_KERNEL_START_VADDR) ? &x86_kpg_infra->pgdir[pgd_i] : &pg->pgdir[pgd_i];
        const x86_pgtable_entry *pgt = (vaddr >= MOS_KERNEL_START_VADDR) ? &x86_kpg_infra->pgtable[pgd_i * 1024 + pgt_i] : &pg->pgtable[pgd_i * 1024 + pgt_i];

        const bool present = pgd->present && pgt->present;
        if (present == previous_present && !present)
        {
            if (!pgd->present && pgt_i == 0)
            {
                vaddr += MOS_PAGE_SIZE * 1024;
                n_pages_left = (n_pages_left > 1024) ? n_pages_left - 1024 : 0;
            }
            else
            {
                vaddr += MOS_PAGE_SIZE;
                n_pages_left--;
            }
            continue;
        }

        const pfn_t pfn = pgt->pfn;
        const vm_flags flags = VM_READ |                                                              //
                               (pgd->writable && pgt->writable ? VM_WRITE : 0) |                      //
                               (pgd->usermode && pgt->usermode ? VM_USER : 0) |                       //
                               (pgd->cache_disabled && pgt->cache_disabled ? VM_CACHE_DISABLED : 0) | //
                               (pgt->global ? VM_GLOBAL : 0);

        // if anything changed, call the callback
        if (present != previous_present || pfn != previous_pfn + 1 || flags != previous_flags)
        {
            if (previous_block.npages > 0 && previous_present)
                callback(&info, &previous_block, previous_pfn - (previous_block.npages - 1), arg);

            previous_block.vaddr = vaddr;
            previous_block.npages = 1;
            previous_block.flags = flags;
            previous_pfn = pfn;
            previous_present = present;
            previous_flags = flags;
        }
        else
        {
            previous_block.npages++;
            previous_pfn = pfn;
        }

        vaddr += MOS_PAGE_SIZE;
        n_pages_left--;
    } while (n_pages_left > 0);

    if (previous_block.npages > 0 && previous_present)
        callback(&info, &previous_block, previous_pfn - (previous_block.npages - 1), arg);
}
