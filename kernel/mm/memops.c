// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/memops.h"

#include "mos/mm/paging/paging.h"
#include "mos/mm/slab.h"
#include "mos/setup.h"

#include <mos/mm/cow.h>
#include <mos/mm/paging/dump.h>
#include <mos/mm/physical/pmm.h>
#include <mos/panic.h>
#include <mos/tasks/task_types.h>
#include <string.h>

void mos_kernel_mm_init(void)
{
    pr_info("initializing kernel memory management");

    mm_cow_init();
    slab_init();

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
}

mm_get_one_page_result_t mm_get_one_zero_page(void)
{
    const pfn_t pfn = pmm_allocate_frames(1, PMM_ALLOC_NORMAL);
    if (pfn == 0)
    {
        mos_warn("Failed to allocate a page");
        return (mm_get_one_page_result_t){ 0 };
    }

    const ptr_t vaddr = mm_get_free_pages(platform_info->kernel_mm, 1, MOS_ADDR_KERNEL_HEAP, VALLOC_DEFAULT);
    if (vaddr == 0)
    {
        mos_warn("Failed to allocate a page (virtual address unavailable)");
        return (mm_get_one_page_result_t){ 0 };
    }

    mm_map_pages(platform_info->kernel_mm, vaddr, pfn, 1, VM_RW | VM_GLOBAL);

    memzero((void *) vaddr, MOS_PAGE_SIZE);
    return (mm_get_one_page_result_t){ .v = vaddr, .p = pfn };
}
