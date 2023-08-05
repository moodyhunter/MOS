// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/cow.h"

#include "mos/interrupt/ipi.h"
#include "mos/mm/mm.h"
#include "mos/mm/paging/paging.h"
#include "mos/mm/paging/table_ops.h"
#include "mos/platform/platform.h"
#include "mos/tasks/task_types.h"

#include <mos/interrupt/ipi.h>
#include <mos/mm/cow.h>
#include <mos/mm/paging/paging.h>
#include <mos/mm/physical/pmm.h>
#include <mos/platform/platform.h>
#include <mos/printk.h>
#include <mos/tasks/process.h>
#include <mos/tasks/task_types.h>
#include <string.h>

static phyframe_t *zero_page;
static pfn_t zero_page_pfn;

static bool cow_fault_handler(vmap_t *map, ptr_t fault_addr, const pagefault_info_t *info)
{
    if (!info->op_write)
        return false;

    // if the block is CoW, it must be writable
    if (!(map->vmflags & VM_WRITE))
        return false;

    fault_addr = ALIGN_DOWN_TO_PAGE(fault_addr);

    // 1. create a new page
    phyframe_t *frame = mm_get_free_page_raw(); // no need to zero it

    // 2. copy the data from the faulting address to the new page
    memcpy((void *) phyframe_va(frame), (void *) fault_addr, MOS_PAGE_SIZE);

    // 3. replace the faulting phypage with the new one
    //    this will increment the refcount of the new page, ...
    //    ...and also decrement the refcount of the old page

    mm_context_t *mmctx = current_mm;
    mm_replace_page_locked(mmctx, fault_addr, phyframe_pfn(frame), map->vmflags);

    ipi_send_all(IPI_TYPE_INVALIDATE_TLB);
    return true;
}

void cow_init(void)
{
    // zero fill on demand (read-only)
    zero_page = mm_get_free_page(); // already zeroed
    zero_page_pfn = phyframe_pfn(zero_page);
}

vmap_t *cow_clone_vmap(mm_context_t *target_mmctx, vmap_t *source_vmap)
{
    const vm_flags original_flags = source_vmap->vmflags;
    mm_flag_pages(source_vmap->mmctx, source_vmap->vaddr, source_vmap->npages, original_flags & ~VM_WRITE); // remove that VM_WRITE flag
    vmap_t *vmap = mm_clone_vmap(source_vmap, target_mmctx, NULL);                                          // already flagged correctly

    vmap->vmflags = original_flags; // use the expected vmflag, not the one in the page table
    return vmap;
}

vmap_t *cow_allocate_zeroed_pages(mm_context_t *mmctx, size_t npages, ptr_t vaddr, valloc_flags allocflags, vm_flags flags)
{
    const vm_flags ro_flags = VM_READ | ((flags & VM_USER) ? VM_USER : 0);

    spinlock_acquire(&mmctx->mm_lock);
    vmap_t *vmap = mm_get_free_vaddr_locked(mmctx, npages, vaddr, allocflags);

    // zero fill the pages
    // TODO: actually we don't need to map at all (we can do it in the #PF handler)
    for (size_t i = 0; i < npages; i++)
        mm_replace_page_locked(mmctx, vaddr + i * MOS_PAGE_SIZE, zero_page_pfn, ro_flags);

    spinlock_release(&mmctx->mm_lock);

    vmap->vmflags = flags;
    vmap->on_fault = cow_fault_handler;
    return vmap;
}
