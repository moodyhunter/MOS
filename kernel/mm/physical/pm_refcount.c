// SPDX-License-Identifier: GPL-3.0-or-later

#define mos_pmm_impl

#include "lib/stdlib.h"
#include "lib/string.h"
#include "mos/mm/kmalloc.h"
#include "mos/mm/physical/pmm.h"
#include "mos/mm/physical/pmm_internal.h"
#include "mos/panic.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"

static list_head pmlist_allocated_rw = LIST_HEAD_INIT(pmlist_allocated_rw);
static spinlock_t pmlist_allocated_lock = SPINLOCK_INIT;
const list_head *const pmlist_allocated = &pmlist_allocated_rw;

/**
 * @brief Inserts a node into the given list, does not merge with existing nodes.
 *
 * @param list The list to insert the node into.
 * @param node The node to insert.
 */
static void insert_node_to_list(list_head *list, pmlist_node_t *node)
{
    list_foreach(pmlist_node_t, current, *list)
    {
        if (current->range.paddr < node->range.paddr)
            continue;

        if (current->range.paddr == node->range.paddr)
            mos_panic("physical memory region " PTR_FMT "-" PTR_FMT " overlaps with existing region " PTR_FMT "-" PTR_FMT, node->range.paddr,
                      node->range.paddr + node->range.npages * MOS_PAGE_SIZE, current->range.paddr, current->range.paddr + current->range.npages * MOS_PAGE_SIZE);

        list_insert_before(current, node);
        return;
    }

    list_node_append(list, list_node(node));
}

void pmm_internal_add_node_to_allocated_list(pmlist_node_t *node)
{
    spinlock_acquire(&pmlist_allocated_lock);
    insert_node_to_list(&pmlist_allocated_rw, node);
    spinlock_release(&pmlist_allocated_lock);
}

void pmm_internal_ref_range(uintptr_t start, size_t n_pages)
{
}

void pmm_internal_unref_range(uintptr_t start, size_t n_pages, pmm_internal_unref_range_callback_t callback, void *arg)
{
}
