// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/cow.hpp"

#include "mos/mm/mm.hpp"
#include "mos/mm/mmstat.hpp"
#include "mos/mm/paging/paging.hpp"
#include "mos/platform/platform.hpp"

#include <mos/interrupt/ipi.hpp>
#include <mos/mm/cow.hpp>
#include <mos/mm/paging/paging.hpp>
#include <mos/mm/physical/pmm.hpp>
#include <mos/platform/platform.hpp>
#include <mos/syslog/printk.hpp>
#include <mos/tasks/process.hpp>
#include <mos/tasks/task_types.hpp>
#include <mos/types.hpp>
#include <mos_string.hpp>

static phyframe_t *_zero_page = NULL;
static phyframe_t *zero_page(void)
{
    if (unlikely(!_zero_page))
    {
        _zero_page = pmm_ref_one(mm_get_free_page());
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

    if (info->is_write)
    {
        // non-present and write, must be a ZoD page
        info->backing_page = mm_get_free_page();
        vmap_stat_inc(vmap, regular);
        return VMFAULT_MAP_BACKING_PAGE;
    }
    else
    {
        info->backing_page = zero_page();
        vmap_stat_inc(vmap, cow);
        return VMFAULT_MAP_BACKING_PAGE_RO;
    }
}

PtrResult<vmap_t> cow_clone_vmap_locked(MMContext *target_mmctx, vmap_t *src_vmap)
{
    // remove that VM_WRITE flag
    mm_flag_pages_locked(src_vmap->mmctx, src_vmap->vaddr, src_vmap->npages, src_vmap->vmflags.erased(VM_WRITE));
    src_vmap->stat.cow += src_vmap->stat.regular;
    src_vmap->stat.regular = 0; // no longer private

    auto dst_vmap = mm_clone_vmap_locked(src_vmap, target_mmctx);
    if (dst_vmap.isErr())
        return dst_vmap.getErr();

    if (!src_vmap->on_fault)
        src_vmap->on_fault = cow_zod_fault_handler;

    dst_vmap->on_fault = src_vmap->on_fault;
    dst_vmap->stat = dst_vmap->stat;
    return dst_vmap;
}

PtrResult<vmap_t> cow_allocate_zeroed_pages(MMContext *mmctx, size_t npages, ptr_t vaddr, VMFlags flags, bool exact)
{
    spinlock_acquire(&mmctx->mm_lock);
    auto vmap = mm_get_free_vaddr_locked(mmctx, npages, vaddr, exact);
    spinlock_release(&mmctx->mm_lock);

    if (vmap.isErr())
        return vmap;

    vmap->vmflags = flags;
    vmap->on_fault = cow_zod_fault_handler;
    return vmap;
}
