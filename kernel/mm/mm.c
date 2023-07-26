// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/mm.h"

#include "mos/mm/cow.h"
#include "mos/mm/paging/dump.h"
#include "mos/mm/paging/table_ops.h"
#include "mos/mm/slab_autoinit.h"
#include "mos/panic.h"
#include "mos/platform/platform.h"
#include "mos/tasks/task_types.h"

#include <string.h>

static slab_t *vmap_cache = NULL;
MOS_SLAB_AUTOINIT("vmap", vmap_cache, vmap_t);

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

vmap_t *mm_new_vmap(vmblock_t block, vmap_content_t content, vmap_flags_t flags)
{
    vmap_t *map = kmemcache_alloc(vmap_cache);
    linked_list_init(list_node(map));
    map->blk = block;
    map->content = content;
    map->flags = flags;
    return map;
}

void mm_attach_vmap(mm_context_t *mmctx, vmap_t *vmap)
{
    // TODO: enforce locking
    // MOS_ASSERT(spinlock_is_locked(&mmctx->mm_lock));

    MOS_ASSERT(vmap->blk.address_space == mmctx);

    // add to the list, sorted by address
    list_foreach(vmap_t, m, mmctx->mmaps)
    {
        if (m->blk.vaddr > vmap->blk.vaddr)
        {
            list_insert_before(m, vmap);
            return;
        }
    }

    list_node_append(&mmctx->mmaps, list_node(vmap)); // nothing?
}

void mm_find_vmap(mm_context_t *mmctx, ptr_t vaddr, vmap_t **out_vmap, size_t *out_offset)
{
    MOS_ASSERT(spinlock_is_locked(&mmctx->mm_lock));

    list_foreach(vmap_t, m, mmctx->mmaps)
    {
        if (m->blk.vaddr <= vaddr && vaddr < m->blk.vaddr + m->blk.npages * MOS_PAGE_SIZE)
        {
            *out_vmap = m;
            *out_offset = vaddr - m->blk.vaddr;
            return;
        }
    }

    *out_vmap = NULL;
    *out_offset = 0;
}
