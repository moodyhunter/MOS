// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/mm.h"

#include "mos/mm/slab_autoinit.h"
#include "mos/platform/platform.h"
#include "mos/tasks/task_types.h"

static slab_t *vmap_cache = NULL;
MOS_SLAB_AUTOINIT("vmap", vmap_cache, vmap_t);

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
