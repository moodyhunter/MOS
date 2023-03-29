// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/mm/paging/paging.h>
#include <mos/platform/platform.h>
#include <mos/printk.h>
#include <mos/x86/mm/paging.h>
#include <mos/x86/mm/paging_impl.h>
#include <mos/x86/x86_platform.h>
#include <string.h>

// defined in enable_paging.asm
extern void x86_enable_paging_impl(ptr_t page_dir);

static x86_pg_infra_t x86_kpg_infra_storage __aligned(MOS_PAGE_SIZE) = { 0 };
static spinlock_t x86_kernel_pgd_lock = SPINLOCK_INIT;

x86_pg_infra_t *const x86_kpg_infra = &x86_kpg_infra_storage;

static void x86_walk_pagetable_dump_callback(const pgt_iteration_info_t *iter_info, const vmblock_t *block, ptr_t block_paddr, void *arg)
{
    MOS_UNUSED(iter_info);
    ptr_t *prev_end_vaddr = (ptr_t *) arg;
    if (block->vaddr - *prev_end_vaddr > MOS_PAGE_SIZE)
    {
        pr_info("  VGROUP: " PTR_FMT, block->vaddr);
    }

    pr_info2("    " PTR_RANGE " -> " PTR_RANGE ", %5zd pages, %c%c%c, %c%c, %s", //
             block->vaddr,                                                       //
             (ptr_t) (block->vaddr + block->npages * MOS_PAGE_SIZE),             //
             block_paddr,                                                        //
             (ptr_t) (block_paddr + block->npages * MOS_PAGE_SIZE),              //
             block->npages,                                                      //
             block->flags & VM_READ ? 'r' : '-',                                 //
             block->flags & VM_WRITE ? 'w' : '-',                                //
             block->flags & VM_EXEC ? 'x' : '-',                                 //
             block->flags & VM_CACHE_DISABLED ? 'C' : '-',                       //
             block->flags & VM_GLOBAL ? 'G' : '-',                               //
             block->flags & VM_USER ? "user" : "kernel"                          //
    );

    *prev_end_vaddr = block->vaddr + block->npages * MOS_PAGE_SIZE;
}

void x86_mm_paging_init(void)
{
    // initialize the page directory
    memzero(x86_kpg_infra, sizeof(x86_pg_infra_t));
    x86_platform.kernel_pgd.pgd = (ptr_t) x86_kpg_infra;
    x86_platform.kernel_pgd.um_page_map = NULL; // a kernel page table does not have a user-mode page map
    x86_platform.kernel_pgd.pgd_lock = &x86_kernel_pgd_lock;
    current_cpu->pagetable = x86_platform.kernel_pgd;
}

void x86_mm_enable_paging(void)
{
    x86_enable_paging_impl(((ptr_t) x86_kpg_infra->pgdir) - MOS_KERNEL_START_VADDR);
    pr_info("paging: enabled");
    x86_dump_pagetable(x86_platform.kernel_pgd);
}

void x86_dump_pagetable(paging_handle_t handle)
{
    pr_info("Page Table:");
    ptr_t tmp = 0;
    x86_mm_walk_page_table(handle, 0, MOS_MAX_VADDR / MOS_PAGE_SIZE, x86_walk_pagetable_dump_callback, &tmp);
}

void x86_mm_walk_page_table(paging_handle_t handle, ptr_t vaddr_start, size_t n_pages, pgt_iteration_callback_t callback, void *arg)
{
    ptr_t vaddr = vaddr_start;
    size_t n_pages_left = n_pages;

    const x86_pg_infra_t *pg = x86_get_pg_infra(handle);
    const pgt_iteration_info_t info = { .address_space = handle, .vaddr_start = vaddr_start, .npages = n_pages };

    ptr_t previous_paddr = 0;
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

        const ptr_t paddr = pgt->phys_addr << 12;
        const vm_flags flags = VM_READ |                                                              //
                               (pgd->writable && pgt->writable ? VM_WRITE : 0) |                      //
                               (pgd->usermode && pgt->usermode ? VM_USER : 0) |                       //
                               (pgd->cache_disabled && pgt->cache_disabled ? VM_CACHE_DISABLED : 0) | //
                               (pgt->global ? VM_GLOBAL : 0);

        // if anything changed, call the callback
        if (present != previous_present || paddr != previous_paddr + MOS_PAGE_SIZE || flags != previous_flags)
        {
            if (previous_block.npages > 0 && previous_present)
                callback(&info, &previous_block, previous_paddr - (previous_block.npages - 1) * MOS_PAGE_SIZE, arg);

            previous_block.vaddr = vaddr;
            previous_block.npages = 1;
            previous_block.flags = flags;
            previous_paddr = paddr;
            previous_present = present;
            previous_flags = flags;
        }
        else
        {
            previous_block.npages++;
            previous_paddr = paddr;
        }

        vaddr += MOS_PAGE_SIZE;
        n_pages_left--;
    } while (n_pages_left > 0);

    if (previous_block.npages > 0 && previous_present)
        callback(&info, &previous_block, previous_paddr - (previous_block.npages - 1) * MOS_PAGE_SIZE, arg);
}
