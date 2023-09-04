// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/cow.h"

#include "mos/interrupt/ipi.h"
#include "mos/mm/mm.h"
#include "mos/mm/mmstat.h"
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
#include <mos/types.h>
#include <string.h>

static phyframe_t *_zero_page = NULL;
static phyframe_t *zero_page(void)
{
    if (unlikely(!_zero_page))
    {
        _zero_page = mm_get_free_page();
        MOS_ASSERT(_zero_page);
        memzero((void *) phyframe_va(_zero_page), MOS_PAGE_SIZE);
    }

    return _zero_page;
}

static vmfault_result_t cow_zod_fault_handler(vmap_t *vmap, ptr_t fault_addr, pagefault_t *info)
{
    MOS_UNUSED(fault_addr);

    if (info->is_present && info->is_write)
    {
        vmap_stat_dec(vmap, cow); // the faulting page is a CoW page
        vmap_stat_inc(vmap, regular);
        return mm_resolve_cow_fault(vmap, fault_addr, info);
    }

    MOS_ASSERT(!info->is_present); // we can't have (present && !write)

    info->backing_page = zero_page();
    vmap_stat_inc(vmap, cow);
    return VMFAULT_MAP_BACKING_PAGE_RO;
}

vmap_t *cow_clone_vmap_locked(mm_context_t *target_mmctx, vmap_t *src_vmap)
{
    // remove that VM_WRITE flag
    mm_flag_pages_locked(src_vmap->mmctx, src_vmap->vaddr, src_vmap->npages, src_vmap->vmflags & ~VM_WRITE);
    src_vmap->stat.cow += src_vmap->stat.regular;
    src_vmap->stat.regular = 0; // no longer private

    vmap_t *dst_vmap = mm_clone_vmap_locked(src_vmap, target_mmctx);

    if (!src_vmap->on_fault)
        src_vmap->on_fault = cow_zod_fault_handler;

    dst_vmap->on_fault = src_vmap->on_fault;
    dst_vmap->stat = dst_vmap->stat;
    return dst_vmap;
}

vmap_t *cow_allocate_zeroed_pages(mm_context_t *mmctx, size_t npages, ptr_t vaddr, valloc_flags allocflags, vm_flags flags)
{
    spinlock_acquire(&mmctx->mm_lock);
    vmap_t *vmap = mm_get_free_vaddr_locked(mmctx, npages, vaddr, allocflags);
    spinlock_release(&mmctx->mm_lock);
    vmap->vmflags = flags;
    vmap->on_fault = cow_zod_fault_handler;
    return vmap;
}
