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

static bool cow_fault_handler(vmap_t *vmap, ptr_t fault_addr, const pagefault_info_t *info)
{
    if (!info->op_write)
        return false;

    if (!info->page_present)
        return false;

    // if the block is CoW, it must be writable
    if (!(vmap->vmflags & VM_WRITE))
        return false;

    fault_addr = ALIGN_DOWN_TO_PAGE(fault_addr);

    // 1. create a new page
    phyframe_t *frame = mm_get_free_page_raw(); // no need to zero it

    // 2. copy the data from the faulting address to the new page
    memcpy((void *) phyframe_va(frame), (void *) fault_addr, MOS_PAGE_SIZE);

    // 3. replace the faulting phypage with the new one
    //    this will increment the refcount of the new page, ...
    //    ...and also decrement the refcount of the old page
    mm_replace_page_locked(current_mm, fault_addr, phyframe_pfn(frame), vmap->vmflags);

    // 4. invalidate the TLB
    ipi_send_all(IPI_TYPE_INVALIDATE_TLB);
    vmap_mstat_dec(vmap, cow, 1);
    return true;
}

void cow_init(void)
{
    // zero fill on demand (read-only)
    zero_page = mm_get_free_page(); // already zeroed
    zero_page_pfn = phyframe_pfn(zero_page);
}

vmap_t *cow_clone_vmap_locked(mm_context_t *target_mmctx, vmap_t *src_vmap)
{
    // remove that VM_WRITE flag
    mm_flag_pages_locked(src_vmap->mmctx, src_vmap->vaddr, src_vmap->npages, src_vmap->vmflags & ~VM_WRITE);

    vmap_t *dst_vmap = mm_clone_vmap_locked(src_vmap, target_mmctx);

    dst_vmap->on_fault = src_vmap->on_fault = cow_fault_handler;
    dst_vmap->stat.n_cow = dst_vmap->stat.n_inmem;
    src_vmap->stat.n_cow = src_vmap->stat.n_inmem;
    return dst_vmap;
}

vmap_t *cow_allocate_zeroed_pages(mm_context_t *mmctx, size_t npages, ptr_t vaddr, valloc_flags allocflags, vm_flags flags)
{
    const vm_flags ro_flags = VM_READ | ((flags & VM_USER) ? VM_USER : 0);

    spinlock_acquire(&mmctx->mm_lock);
    vmap_t *vmap = mm_get_free_vaddr_locked(mmctx, npages, vaddr, allocflags);

    // zero fill the pages
    for (size_t i = 0; i < npages; i++)
        mm_replace_page_locked(mmctx, vmap->vaddr + i * MOS_PAGE_SIZE, zero_page_pfn, ro_flags);

    spinlock_release(&mmctx->mm_lock);

    vmap->vmflags = flags;
    vmap->on_fault = cow_fault_handler;
    vmap->stat.n_cow = npages;
    vmap->stat.n_inmem = npages;

    return vmap;
}
