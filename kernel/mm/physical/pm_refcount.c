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

        const uintptr_t c_start = current->range.paddr;
        const size_t c_size = current->range.npages * MOS_PAGE_SIZE;
        const uintptr_t n_start = node->range.paddr;
        const size_t n_size = node->range.npages * MOS_PAGE_SIZE;

        if (SUBSET_RANGE(n_start, n_size, c_start, c_size) || SUBSET_RANGE(c_start, c_size, n_start, n_size))
            mos_panic("pmm: trying to insert a node that is a subset of an existing node");

        list_insert_before(current, node);
        return;
    }

    list_node_append(list, list_node(node));
}

void pmm_internal_add_node_to_allocated_list(pmlist_node_t *node)
{
    MOS_ASSERT(node->type == PM_RANGE_ALLOCATED || node->type == PM_RANGE_RESERVED);
    spinlock_acquire(&pmlist_allocated_lock);
    insert_node_to_list(&pmlist_allocated_rw, node);
    spinlock_release(&pmlist_allocated_lock);
}

void pmm_internal_ref_range(uintptr_t start, size_t n_pages)
{
#pragma message "TODO: implement pmm_internal_ref_range()"
    pr_warn("pmm_internal_ref_range(" PTR_FMT ", %zu)", start, n_pages);
}

void pmm_internal_unref_range(uintptr_t start, size_t n_pages, pmm_internal_unref_range_callback_t callback, void *arg)
{
#pragma message "TODO: implement pmm_internal_unref_range()"
    MOS_UNUSED(callback);
    MOS_UNUSED(arg);
    pr_warn("pmm_internal_unref_range(" PTR_FMT ", %zu)", start, n_pages);
}
