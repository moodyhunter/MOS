// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/slab.h"

#include <mos/mm/cow.h>
#include <mos/mm/paging/page_ops.h>
#include <mos/mm/physical/pmm.h>
#include <mos/panic.h>
#include <mos/tasks/task_types.h>

void mos_kernel_mm_init(void)
{
    pr_info("initializing kernel memory management");

    mm_cow_init();
    slab_init();
    pmm_switch_to_kheap();

#if MOS_DEBUG_FEATURE(vmm)
    declare_panic_hook(mm_dump_current_pagetable, "Dump page table");
    install_panic_hook(&mm_dump_current_pagetable_holder);
    mm_dump_current_pagetable();
#endif

#if MOS_DEBUG_FEATURE(pmm)
    declare_panic_hook(pmm_dump_lists, "Dump PMM lists");
    install_panic_hook(&pmm_dump_lists_holder);
    pmm_dump_lists();
#endif

#if MOS_DEBUG_FEATURE(liballoc)
    declare_panic_hook(liballoc_dump, "Dump liballoc state");
    install_panic_hook(&liballoc_dump_holder);
    liballoc_dump();
#endif
}

// !! This function is called by liballoc, not intended to be called by anyone else !!
void *liballoc_alloc_page(size_t npages)
{
    const vmblock_t block = mm_alloc_pages(current_cpu->pagetable, npages, MOS_ADDR_KERNEL_HEAP, VALLOC_DEFAULT, VM_RW);
    if (block.vaddr == 0)
        return NULL;

    MOS_ASSERT_X(block.vaddr >= MOS_ADDR_KERNEL_HEAP, "only use this function to free kernel heap pages");

    if (unlikely(block.npages < npages))
        mos_warn("failed to allocate %zu pages", npages);

    return (void *) block.vaddr;
}

// !! This function is called by liballoc, not intended to be called by anyone else !!
bool liballoc_free_page(void *vptr, size_t npages)
{
    MOS_ASSERT_X(vptr >= (void *) MOS_ADDR_KERNEL_HEAP, "only use this function to free kernel heap pages");

    if (unlikely(vptr == NULL))
    {
        mos_warn("freeing NULL pointer");
        return false;
    }
    if (unlikely(npages <= 0))
    {
        mos_warn("freeing negative or zero pages");
        return false;
    }

    mm_unmap_pages(current_cpu->pagetable, (ptr_t) vptr, npages);
    return true;
}
