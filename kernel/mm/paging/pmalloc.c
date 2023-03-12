// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/paging/pmalloc.h"

#include "lib/stdlib.h"
#include "lib/string.h"
#include "mos/mm/kmalloc.h"
#include "mos/panic.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"

// we don't need a lock because early setup is only on BSP
static struct
{
    pmblock_t region;
    bool used;
} pmm_early_region_storage[MOS_MAX_EARLY_MEMREGIONS] = { 0 };

static bool pmm_use_kernel_heap = false;

static spinlock_t pmm_region_lock = SPINLOCK_INIT;
static list_head pmm_free_regions_rw = LIST_HEAD_INIT(pmm_free_regions_rw);
const list_head *const pmm_free_regions = &pmm_free_regions_rw;

#define pmm_alloc_new_block_free(_start, _n_pages, _type)                                                                                                                \
    __extension__({                                                                                                                                                      \
        pmblock_t *blk = pmm_alloc_new_block(_start, _n_pages);                                                                                                          \
        blk->free.type = _type;                                                                                                                                          \
        blk;                                                                                                                                                             \
    })

static pmblock_t *pmm_alloc_new_block(uintptr_t start, size_t n_pages)
{
    pmblock_t *r = NULL;
    if (likely(pmm_use_kernel_heap))
    {
        r = kmalloc(sizeof(pmblock_t)); // zeroed later
    }
    else
    {
        for (size_t i = 0; i < MOS_MAX_EARLY_MEMREGIONS; i++)
        {
            if (pmm_early_region_storage[i].used)
                continue;

            pmm_early_region_storage[i].used = true;
            r = &pmm_early_region_storage[i].region;
            break;
        }
    }

    MOS_ASSERT_X(r, "MOS_MAX_EARLY_MEMREGIONS (%d) is too small!", MOS_MAX_EARLY_MEMREGIONS);

    memzero(r, sizeof(pmblock_t));
    linked_list_init(&r->list_node);
    r->paddr = start;
    r->npages = n_pages;

    return r;
}

static void pmm_dealloc_block(pmblock_t *r)
{
    // check if it's an early region
    for (size_t i = 0; i < MOS_MAX_EARLY_MEMREGIONS; i++)
    {
        if (&pmm_early_region_storage[i].region == r)
        {
            pmm_early_region_storage[i].used = false;
            return;
        }
    }

    // no, it's a dynamically allocated region
    kfree(r);
}

static void pmm_add_free_pages(uintptr_t start, size_t n_pages, pmm_region_type_t type)
{
    const uintptr_t end = start + n_pages * MOS_PAGE_SIZE;
    MOS_ASSERT_X(n_pages > 0, "pmm_add_free_pages() called with n_pages == 0");

    // traverse the list to find the correct insertion point
    spinlock_acquire(&pmm_region_lock);
    list_foreach(pmblock_t, current, pmm_free_regions_rw)
    {
        const uintptr_t cstart = current->paddr;
        const uintptr_t cend = cstart + current->npages * MOS_PAGE_SIZE;

        // detect overlapping regions
        const bool start_in_cregion = cstart <= start && start < cend;
        const bool end_in_cregion = cstart < end && end <= cend;
        const bool enclosing_cregion = start <= cstart && cend <= end;

        if (start_in_cregion || end_in_cregion || enclosing_cregion)
            mos_panic("physical memory region " PTR_FMT "-" PTR_FMT " overlaps with existing region " PTR_FMT "-" PTR_FMT, start, end, cstart, cend);

        if (cstart <= start)
            continue;

// some helper macros to avoid code duplication
#define extend_previous_at_end()  prev->npages += n_pages
#define extend_current_at_start() current->paddr = start, current->npages += n_pages
#define insert_before_current()   list_insert_before(current, pmm_alloc_new_block_free(start, n_pages, type))

        // ! Now we have found the insertion point (i.e., before `current`)
        pmblock_t *prev = list_prev_entry(current, pmblock_t);
        if (list_node(prev) == pmm_free_regions)
        {
            // * `current` is the first list node
            prev = NULL;

            // check if we can extend current at the start
            if (cstart == end)
            {
                extend_current_at_start();
                goto done;
            }

            // otherwise insert a new region before current
            insert_before_current();
            goto done;
        }

        const uintptr_t pstart = prev->paddr;
        const uintptr_t pend = pstart + prev->npages * MOS_PAGE_SIZE;

        // check if we can extend previous at the end
        if (prev && pend == start)
        {
            extend_previous_at_end();

            // continue check if we can also 'fill the gap' between previous and current
            if (cstart == end)
            {
                // also extend the previous region till the end of current
                prev->npages += current->npages;
                list_remove(current); // remove current from the list
                pmm_dealloc_block(current);
                goto done;
            }

            goto done;
        }

        // check if we can extend current at the start
        if (cstart == end)
        {
            extend_current_at_start();
            goto done;
        }

        // otherwise insert a new region before current
        insert_before_current();
        goto done;
    }

#undef extend_previous_at_end
#undef extend_current_at_start
#undef insert_before_current

    // if we get here, we have to insert the new region at the end of the list
    pmblock_t *last = pmm_alloc_new_block(start, n_pages);
    last->free.type = type;
    list_node_append(&pmm_free_regions_rw, list_node(last));

done:
    spinlock_release(&pmm_region_lock);
}

static pmblock_t *pmm_acquire_pages_at(uintptr_t start_addr, size_t npages, pmm_region_type_t type)
{
    const uintptr_t end_addr = start_addr + npages * MOS_PAGE_SIZE;

    mos_debug(pmm, "allocating " PTR_FMT "-" PTR_FMT, start_addr, end_addr);

    spinlock_acquire(&pmm_region_lock);
    list_foreach(pmblock_t, this, pmm_free_regions_rw)
    {
#define this_start (this->paddr)
#define this_end   (this->paddr + this->npages * MOS_PAGE_SIZE)

        // if this region is completely before the region we want to remove, skip it
        if (start_addr < this_start || this_end < end_addr)
            continue;

        MOS_ASSERT_X(this->free.type == type, "pmm_acquire_pages_at(): region type mismatch");

        //
        //       |-> Start of this region
        //       |               End of this region <-|
        // ======|====================================|======
        //  PREV | PART 1 | REGION TO REMOVE | PART 2 | NEXT
        // ======|========|==================|========|======
        //                |-> start_addr     |
        //                        end_addr <-|
        //
        const size_t part_1_size = start_addr - this_start;
        const size_t part_2_size = this_end - end_addr;

        if (part_1_size == 0 && part_2_size != 0)
        {
            // part 1 is empty, which means we are removing from the front of the region
            this->paddr = end_addr;
            this->npages = part_2_size / MOS_PAGE_SIZE;
            mos_debug(pmm, "case 1: shrink " PTR_FMT ", new_size=%zd", this_start, this->npages);
        }
        else if (part_1_size != 0 && part_2_size == 0)
        {
            // part 2 is empty, which means we are removing the tail of part 1
            mos_debug(pmm, "case 2: shrink " PTR_FMT "-" PTR_FMT ": new_end=" PTR_FMT, this_start, this_end, this_start + (this->npages - npages) * MOS_PAGE_SIZE);
            this->npages -= npages;
        }
        else if (part_1_size == 0 && part_2_size == 0)
        {
            // both part 1 and part 2 are empty, which means we are removing the whole region
            mos_debug(pmm, "case 3: remove block " PTR_FMT "-" PTR_FMT, this_start, this_end);

            // remove it from the list
            list_remove(this);
            pmm_dealloc_block(this);
        }
        else
        {
            // neither part 1 nor part 2 is empty, so we have to allocate a new entry for part 2
            mos_debug(pmm, "case 4: split " PTR_FMT "-" PTR_FMT "->" PTR_FMT "-" PTR_FMT "," PTR_FMT "-" PTR_FMT, this_start, this_end, this_start, start_addr, end_addr,
                      end_addr + part_2_size);
            this->npages = part_1_size / MOS_PAGE_SIZE;
            pmblock_t *new = pmm_alloc_new_block_free(end_addr, part_2_size / MOS_PAGE_SIZE, this->free.type);
            list_insert_after(this, new); // insert after this
        }

        spinlock_release(&pmm_region_lock);

        // allocate the new block
        // no matter how we split the region, we always allocate the region
        // from start_addr to end_addr
        pmblock_t *new = pmm_alloc_new_block(start_addr, npages);
        new->allocated.refcount = 0;
        return new;

#undef this_start
#undef this_end
    }

    spinlock_release(&pmm_region_lock);
    mos_panic("cannot allocate contiguous pages " PTR_FMT "-" PTR_FMT, start_addr, end_addr);
}

void pmm_dump(void)
{
    pr_info("Physical Memory Manager dump:");
    char sbuf[32];

    size_t i = 0;
    list_foreach(pmblock_t, r, add_const(pmm_free_regions_rw))
    {
        format_size(sbuf, sizeof(sbuf), r->npages * MOS_PAGE_SIZE);
        const uintptr_t end = r->paddr + r->npages * MOS_PAGE_SIZE - 1;
        const char *const type = r->free.type == PMM_REGION_USEABLE ? "available" : "reserved";
        pr_info("%2zd: " PTR_FMT "-" PTR_FMT " (%zu page(s), %s, %s)", i, r->paddr, end, r->npages, sbuf, type);
        i++;
    }
}

void pmm_switch_to_kheap(void)
{
    MOS_ASSERT_X(!pmm_use_kernel_heap, "pmm_switch_to_kheap() called twice");
    pmm_use_kernel_heap = true;
    pr_info("pmm: switched to kernel heap");
}

void mos_pmm_add_region(uintptr_t start_addr, size_t nbytes, pmm_region_type_t type)
{
    // TODO: should we align up to page size?
    const uintptr_t start = ALIGN_UP_TO_PAGE(start_addr);
    const uintptr_t end = ALIGN_UP_TO_PAGE(start_addr + nbytes);
    const size_t npages = (end - start) / MOS_PAGE_SIZE;

    if (unlikely(npages == 0))
    {
        pr_warn("physical memory region " PTR_FMT "-" PTR_FMT " is empty after alignment", start, end);
        return;
    }

    const size_t loss = (start - start_addr) + (end - (start_addr + nbytes));
    if (unlikely(loss))
        pr_warn("physical memory region " PTR_FMT "-" PTR_FMT " is not page-aligned, losing %zu bytes", start_addr, start_addr + nbytes, loss);

    if (unlikely(end < 1 MB))
    {
        type = PMM_REGION_RESERVED;
        pr_info2("reserving a low memory region " PTR_FMT "-" PTR_FMT " (%zu page(s))", start, end, npages);
    }

    pmm_add_free_pages(start, npages, type);
}

pmblock_t *pmm_allocate(size_t n_pages)
{
    mos_debug(pmm, "allocating %zu page(s)", n_pages);

    pmblock_t *result = NULL;
    size_t n_allocated = 0;

#define n_left() (n_pages - n_allocated)
    spinlock_acquire(&pmm_region_lock);
    list_foreach(pmblock_t, current, pmm_free_regions_rw)
    {
    // this is necessary because we might remove the current node from the list
    // and we don't want to skip the next node
    next_node:
        // check if we are at the end of the list, or if we have allocated enough pages
        if (list_node(current) == &pmm_free_regions_rw || n_left() == 0)
            break;

        // skip reserved regions (of course!)
        if (current->free.type != PMM_REGION_USEABLE)
            continue;

        // check if we can allocate the whole region
        if (current->npages <= n_left())
        {
            mos_debug(pmm, "  %8s: " PTR_FMT "-" PTR_FMT " (%zu page(s))", "whole", current->paddr, current->paddr + current->npages * MOS_PAGE_SIZE, current->npages);
            pmblock_t *next = list_next_entry(current, pmblock_t);
            list_remove(current);
            n_allocated += current->npages;

            if (result == NULL)
                result = current;
            else
                list_append(result, current);

            current = next;
            goto next_node;
        }

        // only allocate a part of the region
        MOS_ASSERT(current->npages > n_left()); // of course, otherwise we would have allocated the whole region

        size_t n_left = n_left(); // number of pages left to allocate, don't use the macro here!

        pmblock_t *new = pmm_alloc_new_block(current->paddr, n_left);
        if (result == NULL)
            result = new;
        else
            list_append(result, new);
        n_allocated += n_left;
        current->paddr += n_left * MOS_PAGE_SIZE;
        current->npages -= n_left;
        mos_debug(pmm, "  %8s: " PTR_FMT "-" PTR_FMT " (%zu page(s))", "partial", new->paddr, new->paddr + new->npages *MOS_PAGE_SIZE, new->npages);
    }
    spinlock_release(&pmm_region_lock);
#undef n_left

    if (n_allocated != n_pages)
        mos_panic("could not allocate %zu pages, only allocated %zu pages", n_pages, n_allocated);

    list_headless_foreach(pmblock_t, current, *result)
    {
        current->allocated.refcount = 0;
    }

    return result;
}

uintptr_t pmm_get_page_paddr(const pmblock_t *blocks, size_t page_idx)
{
    MOS_ASSERT_X(blocks, "NULL pointer in physical memory block list");

    size_t n = 0;
    list_headless_foreach(pmblock_t, current, add_const(*blocks))
    {
        if (n + current->npages > page_idx)
            return current->paddr + (page_idx - n) * MOS_PAGE_SIZE;

        n += current->npages;
    }

    mos_panic("page index %zu out of bounds", page_idx);
}

void pmm_free(pmblock_t *blocks)
{
    list_headless_foreach(pmblock_t, block, *blocks)
    {
        const uintptr_t start_addr = block->paddr;
        const size_t npages = block->npages;
        pmm_dealloc_block(block);
        pmm_add_free_pages(start_addr, npages, PMM_REGION_USEABLE);
    }
}

pmblock_t *pmm_acquire_usable_pages_at(uintptr_t start_addr, size_t npages)
{
    return pmm_acquire_pages_at(start_addr, npages, PMM_REGION_USEABLE);
}

pmblock_t *pmm_acquire_reserved_pages_at(uintptr_t start_addr, size_t npages)
{
    return pmm_acquire_pages_at(start_addr, npages, PMM_REGION_RESERVED);
}
