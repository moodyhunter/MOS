// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/memops.h"

#include "mos/mm/paging/paging.h"
#include "mos/mm/paging/table_ops.h"
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
}

phyframe_t *mm_get_free_page_raw(void)
{
    phyframe_t *frame = pmm_allocate_frames(1, PMM_ALLOC_NORMAL);
    if (!frame)
    {
        mos_warn("Failed to allocate a page");
        return NULL;
    }

    return frame;
}

phyframe_t *mm_get_free_page(void)
{
    phyframe_t *frame = mm_get_free_page_raw();
    const ptr_t vaddr = phyframe_va(frame);
    memzero((void *) vaddr, MOS_PAGE_SIZE);
    return frame;
}

phyframe_t *mm_get_free_pages(size_t npages)
{
    phyframe_t *frame = pmm_allocate_frames(npages, PMM_ALLOC_NORMAL);
    if (!frame)
    {
        mos_warn("Failed to allocate %zd pages", npages);
        return NULL;
    }

    return frame;
}

void mm_unref_pages(phyframe_t *frame, size_t npages)
{
    pmm_unref_frames(phyframe_pfn(frame), npages);
}
